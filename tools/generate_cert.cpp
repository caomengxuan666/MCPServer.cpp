#ifdef _WIN32
extern "C" {
#include <windows.h>
// for only windows
#include <openssl/applink.c>
#include <wincrypt.h>
}
#pragma comment(lib, "crypt32.lib")
#endif

#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include "openssl/dh.h"

#include <cstring>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>
namespace fs = std::filesystem;

// Version information
const std::string VERSION = "2.0.0";

// Configuration structure
struct Config {
    std::string cert_dir = "certs";
    std::string server_cert_file = "server.crt";
    std::string server_key_file = "server.key";
    std::string ca_cert_file = "ca.crt";
    std::string ca_key_file = "ca.key";
    std::string dh_file = "dh2048.pem";
    int bits = 2048;
    int ca_days = 3650;// CA validity longer
    int server_days = 365;
    std::string country = "US";
    std::string organization = "MCPServer++";
    std::string common_name = "localhost";
    std::vector<std::string> dns_names;
    std::vector<std::string> ip_addresses;
    bool show_help = false;
    bool show_version = false;
    bool install_trust = false;// New: whether to install to trust store
};

void show_usage(const char *program_name) {
    std::cout << "SSL Certificate Generator v" << VERSION << "\n";
    std::cout << "Usage: " << program_name << " [OPTIONS]\n\n";
    std::cout << "Generates a full PKI environment with CA and server certificates.\n\n";
    std::cout << "Optional arguments:\n";
    std::cout << "  -h, --help           Show this help message and exit\n";
    std::cout << "  -v, --version        Show version information and exit\n";
    std::cout << "  -d, --dir DIR        Output directory (default: certs)\n";
    std::cout << "  --install-trust      Install CA to system trust store (Windows/Linux)\n";
    std::cout << "  --dns DNS            Add DNS SAN (e.g., --dns localhost --dns 127.0.0.1)\n";
    std::cout << "  --ip IP              Add IP SAN (e.g., --ip 127.0.0.1)\n";
    std::cout << "  -CN, --common-name   Common Name (default: localhost)\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " --install-trust\n";
    std::cout << "  " << program_name << " --dns myserver.com --ip 192.168.1.100\n";
}

void show_version() {
    std::cout << "SSL Certificate Generator v" << VERSION << "\n";
}

// Add progress display function
void show_progress(const std::string &step, int current, int total) {
    int progress = (current * 100) / total;
    std::cout << "\r[" << std::string(progress / 2, '=') << std::string(50 - progress / 2, ' ')
              << "] " << progress << "% " << step << std::flush;
}

bool is_valid_ip(const std::string &ip) {
    const std::regex ipv4_regex(R"(^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$)");
    const std::regex ipv6_regex(R"(^([0-9a-fA-F]{1,4}:){7}[0-9a-fA-F]{1,4}$|::([0-9a-fA-F]{1,4}:){0,5}[0-9a-fA-F]{1,4}$)");
    return std::regex_match(ip, ipv4_regex) || std::regex_match(ip, ipv6_regex);
}

bool is_valid_dns(const std::string &dns) {
    const std::regex dns_regex(R"(^[a-zA-Z0-9]([a-zA-Z0-9\-]{0,61}[a-zA-Z0-9])?(\.[a-zA-Z0-9]([a-zA-Z0-9\-]{0,61}[a-zA-Z0-9])?)*$)");
    return std::regex_match(dns, dns_regex) && dns.length() <= 255;
}

Config parse_args(int argc, char *argv[]) {
    Config cfg;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            cfg.show_help = true;
            return cfg;
        }
        if (arg == "-v" || arg == "--version") {
            cfg.show_version = true;
            return cfg;
        }
        if (arg == "-d" || arg == "--dir") {
            if (++i >= argc) throw std::runtime_error("--dir requires arg");
            cfg.cert_dir = argv[i];
        } else if (arg == "--install-trust") {
            cfg.install_trust = true;
        } else if (arg == "--dns") {
            if (++i >= argc) throw std::runtime_error("--dns requires arg");
            std::string dns = argv[i];
            if (!is_valid_dns(dns)) throw std::runtime_error("Invalid DNS: " + dns);
            cfg.dns_names.push_back(dns);
        } else if (arg == "--ip") {
            if (++i >= argc) throw std::runtime_error("--ip requires arg");
            std::string ip = argv[i];
            if (!is_valid_ip(ip)) throw std::runtime_error("Invalid IP: " + ip);
            cfg.ip_addresses.push_back(ip);
        } else if (arg == "-CN" || arg == "--common-name") {
            if (++i >= argc) throw std::runtime_error("--common-name requires arg");
            cfg.common_name = argv[i];
        } else {
            throw std::runtime_error("Unknown arg: " + arg);
        }
    }
    if (cfg.dns_names.empty() && cfg.ip_addresses.empty()) {
        cfg.dns_names = {"localhost", "127.0.0.1"};
        cfg.ip_addresses = {"127.0.0.1"};
    }
    return cfg;
}

EVP_PKEY *generate_rsa_key(int bits) {
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    if (!ctx || EVP_PKEY_keygen_init(ctx) <= 0 || EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, bits) <= 0) {
        ERR_print_errors_fp(stderr);
        EVP_PKEY_CTX_free(ctx);
        return nullptr;
    }
    EVP_PKEY *pkey = nullptr;
    if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
        ERR_print_errors_fp(stderr);
        pkey = nullptr;
    }
    EVP_PKEY_CTX_free(ctx);
    return pkey;
}

X509_EXTENSION *create_san_extension(const Config &cfg) {
    std::string san_str;
    for (size_t i = 0; i < cfg.dns_names.size(); ++i) san_str += (i ? "," : "") + std::string("DNS:") + cfg.dns_names[i];
    for (size_t i = 0; i < cfg.ip_addresses.size(); ++i) san_str += (san_str.empty() ? "" : ",") + std::string("IP:") + cfg.ip_addresses[i];
    X509V3_CTX ctx;
    X509V3_set_ctx_nodb(&ctx);
    return X509V3_EXT_conf_nid(nullptr, &ctx, NID_subject_alt_name, const_cast<char *>(san_str.c_str()));
}

X509 *create_certificate(EVP_PKEY *pkey, EVP_PKEY *ca_pkey, X509 *ca_x509, const Config &cfg, bool is_ca) {
    X509 *x509 = X509_new();
    if (!x509) return nullptr;

    X509_set_version(x509, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);
    X509_gmtime_adj(X509_get_notBefore(x509), 0);
    X509_gmtime_adj(X509_get_notAfter(x509), (long) 60 * 60 * 24 * (is_ca ? cfg.ca_days : cfg.server_days));
    X509_set_pubkey(x509, pkey);

    X509_NAME *name = X509_get_subject_name(x509);
    X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, (unsigned char *) cfg.country.c_str(), -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC, (unsigned char *) cfg.organization.c_str(), -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (unsigned char *) (is_ca ? "MCPServer++ Root CA" : cfg.common_name.c_str()), -1, -1, 0);

    if (is_ca) {
        X509_set_issuer_name(x509, name);
    } else {
        X509_set_issuer_name(x509, X509_get_subject_name(ca_x509));
    }

    if (!is_ca) {
        X509_EXTENSION *san_ext = create_san_extension(cfg);
        if (san_ext) {
            X509_add_ext(x509, san_ext, -1);
            X509_EXTENSION_free(san_ext);
        }
    }

    X509_EXTENSION *bc_ext = X509V3_EXT_conf_nid(nullptr, nullptr, NID_basic_constraints, is_ca ? "CA:TRUE" : "CA:FALSE");
    if (bc_ext) {
        X509_add_ext(x509, bc_ext, -1);
        X509_EXTENSION_free(bc_ext);
    }

    if (X509_sign(x509, is_ca ? pkey : ca_pkey, EVP_sha256()) <= 0) {
        X509_free(x509);
        return nullptr;
    }
    return x509;
}

bool save_key(EVP_PKEY *pkey, const std::string &path) {
    FILE *fp = fopen(path.c_str(), "w");
    if (!fp) return false;
    bool ok = PEM_write_PrivateKey(fp, pkey, nullptr, nullptr, 0, nullptr, nullptr) > 0;
    fclose(fp);
    return ok;
}

bool save_cert(X509 *x509, const std::string &path) {
    FILE *fp = fopen(path.c_str(), "w");
    if (!fp) return false;
    bool ok = PEM_write_X509(fp, x509) > 0;
    fclose(fp);
    return ok;
}

// Generate DH parameters
bool generate_dh_params(const std::string &path, int bits = 2048) {
    std::cout << "\nGenerating DH parameters (this may take a while)";
    std::cout.flush();

    DH *dh = DH_new();
    if (!dh) return false;

    // Use callback function to show progress
    BN_GENCB *cb = BN_GENCB_new();
    if (!cb) {
        DH_free(dh);
        return false;
    }

    // Dot progress indicator callback
    auto callback = [](int p, int n, BN_GENCB *cb) -> int {
        if (p == 1) {// Iteration
            std::cout << "." << std::flush;
        }
        return 1;// Continue execution
    };

    BN_GENCB_set(cb, callback, nullptr);

    bool result = DH_generate_parameters_ex(dh, bits, 2, cb) != 0;

    if (!result) {
        DH_free(dh);
        BN_GENCB_free(cb);
        std::cout << " failed!\n";
        return false;
    }

    FILE *fp = fopen(path.c_str(), "w");
    if (!fp) {
        DH_free(dh);
        BN_GENCB_free(cb);
        std::cout << " failed!\n";
        return false;
    }

    bool ok = PEM_write_DHparams(fp, dh) > 0;
    fclose(fp);
    DH_free(dh);
    BN_GENCB_free(cb);

    if (ok) {
        std::cout << " Done!\n";
    } else {
        std::cout << " failed!\n";
    }
    return ok;
}


// Windows: Install CA to local machine trust store
#ifdef _WIN32
bool install_ca_windows(const std::string &ca_path) {
    // Fix CertOpenSystemStoreA call, use 0 instead of nullptr
    HANDLE hStore = CertOpenSystemStoreA(0, "ROOT");
    if (!hStore) return false;

    FILE *fp = fopen(ca_path.c_str(), "rb");
    if (!fp) {
        CertCloseStore(hStore, 0);
        return false;
    }

    X509 *x509 = PEM_read_X509(fp, nullptr, nullptr, nullptr);
    fclose(fp);
    if (!x509) {
        CertCloseStore(hStore, 0);
        return false;
    }

    // Fix X509* to PCCERT_CONTEXT conversion issue
    // First convert X509 to DER format
    unsigned char *der_buf = nullptr;
    int der_len = i2d_X509(x509, &der_buf);
    if (der_len < 0) {
        X509_free(x509);
        CertCloseStore(hStore, 0);
        return false;
    }

    // Create certificate context
    PCCERT_CONTEXT cert_ctx = CertCreateCertificateContext(
            X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
            der_buf,
            der_len);

    // Free DER buffer
    OPENSSL_free(der_buf);
    X509_free(x509);

    if (!cert_ctx) {
        CertCloseStore(hStore, 0);
        return false;
    }

    // Use correct certificate context
    bool ok = CertAddCertificateContextToStore(hStore, cert_ctx, CERT_STORE_ADD_REPLACE_EXISTING, nullptr) != 0;

    // Clean up certificate context
    CertFreeCertificateContext(cert_ctx);
    CertCloseStore(hStore, 0);
    return ok;
}
#endif

// Linux: Install CA to system trust store
bool install_ca_linux(const std::string &ca_path) {
    fs::path dest = "/usr/local/share/ca-certificates/MCPServer++.crt";
    if (std::system(("cp \"" + ca_path + "\" \"" + dest.string() + "\"").c_str()) != 0) return false;
    return std::system("update-ca-certificates") == 0;
}

int main(int argc, char *argv[]) {
#ifdef _WIN32
    OPENSSL_Applink();
#endif

    try {
        Config cfg = parse_args(argc, argv);
        if (cfg.show_help) {
            show_usage(argv[0]);
            return 0;
        }
        if (cfg.show_version) {
            show_version();
            return 0;
        }

        OpenSSL_add_all_algorithms();
        ERR_load_crypto_strings();
        X509V3_add_standard_extensions();

        if (!fs::exists(cfg.cert_dir)) fs::create_directories(cfg.cert_dir);

        fs::path dir = cfg.cert_dir;
        fs::path ca_cert = dir / cfg.ca_cert_file;
        fs::path ca_key = dir / cfg.ca_key_file;
        fs::path server_cert = dir / cfg.server_cert_file;
        fs::path server_key = dir / cfg.server_key_file;
        fs::path dh_file = dir / cfg.dh_file;

        std::cout << "Generating full PKI environment...\n";
        std::cout << "Output directory: " << dir << "\n";

        // 1. Generate CA
        show_progress("Generating CA key...", 1, 5);
        EVP_PKEY *ca_pkey = generate_rsa_key(cfg.bits);
        if (!ca_pkey) {
            std::cerr << "\n× Failed to generate CA key\n";
            return 1;
        }

        show_progress("Generating CA certificate...", 2, 5);
        X509 *ca_x509 = create_certificate(ca_pkey, nullptr, nullptr, cfg, true);
        if (!ca_x509) {
            std::cerr << "\n× Failed to generate CA certificate\n";
            EVP_PKEY_free(ca_pkey);
            return 1;
        }
        save_key(ca_pkey, ca_key.string());
        save_cert(ca_x509, ca_cert.string());

        // 2. Generate Server Cert
        show_progress("Generating server key...", 3, 5);
        EVP_PKEY *server_pkey = generate_rsa_key(cfg.bits);
        if (!server_pkey) {
            std::cerr << "\n× Failed to generate server key\n";
            X509_free(ca_x509);
            EVP_PKEY_free(ca_pkey);
            return 1;
        }

        show_progress("Generating server certificate...", 4, 5);
        X509 *server_x509 = create_certificate(server_pkey, ca_pkey, ca_x509, cfg, false);
        if (!server_x509) {
            std::cerr << "\n× Failed to generate server certificate\n";
            EVP_PKEY_free(server_pkey);
            X509_free(ca_x509);
            EVP_PKEY_free(ca_pkey);
            return 1;
        }
        save_key(server_pkey, server_key.string());
        save_cert(server_x509, server_cert.string());

        // 3. Generate DH
        show_progress("Generating DH parameters (this may take a while)...", 5, 5);
        if (!generate_dh_params(dh_file.string())) {
            std::cerr << "\n   Warning: Failed to generate DH parameters\n";
        }

        // Clear progress line
        std::cout << "\r" << std::string(80, ' ') << "\r" << std::flush;

        // 4. Install to trust store
        if (cfg.install_trust) {
            std::cout << "Installing CA to system trust store...\n";
#ifdef _WIN32
            if (install_ca_windows(ca_cert.string())) {
                std::cout << "√ CA certificate installed to Windows trust store\n";
            } else {
                std::cerr << "× Failed to install CA to Windows trust store (Run as Admin?)\n";
            }
#elif defined(__linux__)
            if (install_ca_linux(ca_cert.string())) {
                std::cout << "√ CA certificate installed to Linux trust store\n";
            } else {
                std::cerr << "× Failed to install CA to Linux trust store (Need sudo?)\n";
            }
#endif
        }

        // Cleanup
        X509_free(ca_x509);
        X509_free(server_x509);
        EVP_PKEY_free(ca_pkey);
        EVP_PKEY_free(server_pkey);
        ERR_free_strings();
        EVP_cleanup();

        std::cout << "\n√ Successfully generated:\n";
        std::cout << "  CA Certificate: " << ca_cert << "\n";
        std::cout << "  CA Key:         " << ca_key << "\n";
        std::cout << "  Server Cert:    " << server_cert << "\n";
        std::cout << "  Server Key:     " << server_key << "\n";
        std::cout << "  DH Params:      " << dh_file << "\n\n";
        std::cout << "Use in your server:\n";
        std::cout << "  cert = \"" << server_cert << "\", key = \"" << server_key << "\"\n";
        std::cout << "  dh_file = \"" << dh_file << "\"\n";
        if (!cfg.install_trust) {
            std::cout << "\n💡 Tip: Use --install-trust to auto-install CA to system trust\n";
        }

        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}