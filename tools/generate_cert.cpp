#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>


// OpenSSL includes
#ifdef _WIN32
#include <openssl/applink.c>
#endif

#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

#ifdef _WIN32
#include <direct.h>
#define GETCWD _getcwd
#else
#include <unistd.h>
#define GETCWD getcwd
#endif

namespace fs = std::filesystem;

void show_usage(const char *program_name) {
    std::cout << "SSL Certificate Generator for MCPServer++\n";
    std::cout << "========================================\n\n";
    std::cout << "This tool generates a self-signed SSL certificate and private key for testing HTTPS functionality.\n\n";
    std::cout << "Usage: " << program_name << " [options]\n";
    std::cout << "Options:\n";
    std::cout << "  -h, --help              Show this help message\n";
    std::cout << "  -d, --dir DIR           Directory to generate certificates in (default: certs)\n";
    std::cout << "  -c, --cert FILE         Certificate file name (default: server.crt)\n";
    std::cout << "  -k, --key FILE          Private key file name (default: server.key)\n";
    std::cout << "  -b, --bits BITS         RSA key size in bits (default: 2048)\n";
    std::cout << "\nExample:\n";
    std::cout << "  " << program_name << " -d certs -c server.crt -k server.key\n";
    std::cout << "  " << program_name << " --dir /path/to/certs --cert mycert.pem --key mykey.pem\n";
    std::cout << "\nAfter generating the certificate, update your config.ini:\n";
    std::cout << "  enable_https=1\n";
    std::cout << "  ssl_cert_file=certs/server.crt\n";
    std::cout << "  ssl_key_file=certs/server.key\n";
}

// Function to generate RSA key pair
EVP_PKEY *generate_rsa_key(int bits) {
    EVP_PKEY *pkey = nullptr;
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);

    if (!ctx) {
        return nullptr;
    }

    if (EVP_PKEY_keygen_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return nullptr;
    }

    if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, bits) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return nullptr;
    }

    if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return nullptr;
    }

    EVP_PKEY_CTX_free(ctx);
    return pkey;
}

// Function to create a self-signed certificate
X509 *create_self_signed_certificate(EVP_PKEY *pkey, int days) {
    X509 *x509 = X509_new();
    if (!x509) {
        return nullptr;
    }

    // Set version (version 3 certificate)
    X509_set_version(x509, 2);

    // Generate and set serial number
    ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);

    // Set validity period
    X509_gmtime_adj(X509_get_notBefore(x509), 0);
    X509_gmtime_adj(X509_get_notAfter(x509), (long) 60 * 60 * 24 * days);

    // Set public key
    X509_set_pubkey(x509, pkey);

    // Get subject name
    X509_NAME *name = X509_get_subject_name(x509);

    // Set certificate fields
    X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, (unsigned char *) "US", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC, (unsigned char *) "MCPServer++", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (unsigned char *) "localhost", -1, -1, 0);

    // Set issuer name (self-signed, so same as subject)
    X509_set_issuer_name(x509, name);

    // Sign the certificate
    if (X509_sign(x509, pkey, EVP_sha256()) <= 0) {
        X509_free(x509);
        return nullptr;
    }

    return x509;
}

// Function to save private key to file
bool save_private_key(EVP_PKEY *pkey, const std::string &filename) {
    FILE *fp = fopen(filename.c_str(), "w");
    if (!fp) {
        return false;
    }

    if (PEM_write_PrivateKey(fp, pkey, nullptr, nullptr, 0, nullptr, nullptr) <= 0) {
        fclose(fp);
        return false;
    }

    fclose(fp);
    return true;
}

// Function to save certificate to file
bool save_certificate(X509 *x509, const std::string &filename) {
    FILE *fp = fopen(filename.c_str(), "w");
    if (!fp) {
        return false;
    }

    if (PEM_write_X509(fp, x509) <= 0) {
        fclose(fp);
        return false;
    }

    fclose(fp);
    return true;
}

int main(int argc, char *argv[]) {
#ifdef _WIN32  
    OPENSSL_Applink();
#endif
    // Default values
    std::string cert_dir = "certs";
    std::string cert_file = "server.crt";
    std::string key_file = "server.key";
    int bits = 2048;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            show_usage(argv[0]);
            return 0;
        } else if (arg == "-d" || arg == "--dir") {
            if (i + 1 < argc) {
                cert_dir = argv[++i];
            } else {
                std::cerr << "Error: --dir requires a directory argument\n";
                return 1;
            }
        } else if (arg == "-c" || arg == "--cert") {
            if (i + 1 < argc) {
                cert_file = argv[++i];
            } else {
                std::cerr << "Error: --cert requires a file name argument\n";
                return 1;
            }
        } else if (arg == "-k" || arg == "--key") {
            if (i + 1 < argc) {
                key_file = argv[++i];
            } else {
                std::cerr << "Error: --key requires a file name argument\n";
                return 1;
            }
        } else if (arg == "-b" || arg == "--bits") {
            if (i + 1 < argc) {
                try {
                    bits = std::stoi(argv[++i]);
                    if (bits < 1024) {
                        std::cerr << "Warning: Key size less than 1024 bits is not recommended\n";
                    }
                } catch (...) {
                    std::cerr << "Error: --bits requires a numeric argument\n";
                    return 1;
                }
            } else {
                std::cerr << "Error: --bits requires a numeric argument\n";
                return 1;
            }
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            show_usage(argv[0]);
            return 1;
        }
    }

    // Initialize OpenSSL
    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();

    // Create certificate directory if it doesn't exist
    std::error_code ec;
    if (!fs::exists(cert_dir, ec)) {
        if (!fs::create_directories(cert_dir, ec)) {
            std::cerr << "Error: Failed to create directory '" << cert_dir << "': " << ec.message() << "\n";
            ERR_free_strings();
            EVP_cleanup();
            return 1;
        }
        std::cout << "Created directory: " << cert_dir << "\n";
    } else if (!fs::is_directory(cert_dir)) {
        std::cerr << "Error: '" << cert_dir << "' exists but is not a directory\n";
        ERR_free_strings();
        EVP_cleanup();
        return 1;
    }

    // Full paths to certificate and key files
    fs::path cert_path = fs::path(cert_dir) / cert_file;
    fs::path key_path = fs::path(cert_dir) / key_file;

    // Check if files already exist
    if (fs::exists(cert_path)) {
        std::cout << "Certificate file already exists: " << cert_path << "\n";
        std::cout << "Skipping certificate generation\n";
        ERR_free_strings();
        EVP_cleanup();
        return 0;
    }

    if (fs::exists(key_path)) {
        std::cout << "Private key file already exists: " << key_path << "\n";
        std::cout << "Skipping certificate generation\n";
        ERR_free_strings();
        EVP_cleanup();
        return 0;
    }

    std::cout << "Generating SSL certificate and private key...\n";
    std::cout << "Key size: " << bits << " bits\n";

    // Generate RSA key pair
    EVP_PKEY *pkey = generate_rsa_key(bits);
    if (!pkey) {
        std::cerr << "Error: Failed to generate RSA key pair\n";
        ERR_print_errors_fp(stderr);
        ERR_free_strings();
        EVP_cleanup();
        return 1;
    }

    // Create self-signed certificate
    X509 *x509 = create_self_signed_certificate(pkey, 365);
    if (!x509) {
        std::cerr << "Error: Failed to create self-signed certificate\n";
        ERR_print_errors_fp(stderr);
        EVP_PKEY_free(pkey);
        ERR_free_strings();
        EVP_cleanup();
        return 1;
    }

    // Save private key to file
    if (!save_private_key(pkey, key_path.string())) {
        std::cerr << "Error: Failed to save private key to " << key_path << "\n";
        X509_free(x509);
        EVP_PKEY_free(pkey);
        ERR_free_strings();
        EVP_cleanup();
        return 1;
    }

    // Save certificate to file
    if (!save_certificate(x509, cert_path.string())) {
        std::cerr << "Error: Failed to save certificate to " << cert_path << "\n";
        X509_free(x509);
        EVP_PKEY_free(pkey);
        ERR_free_strings();
        EVP_cleanup();
        return 1;
    }

    // Clean up
    X509_free(x509);
    EVP_PKEY_free(pkey);
    ERR_free_strings();
    EVP_cleanup();

    std::cout << "Successfully generated:\n";
    std::cout << "  Certificate: " << cert_path << "\n";
    std::cout << "  Private Key: " << key_path << "\n";
    std::cout << "\n";
    std::cout << "To use these files with MCPServer++, update your config.ini:\n";
    std::cout << "  enable_https=1\n";
    std::cout << "  ssl_cert_file=" << cert_path.string() << "\n";
    std::cout << "  ssl_key_file=" << key_path.string() << "\n";
    std::cout << "\n";
    std::cout << "Note: This is a self-signed certificate for testing purposes only.\n";
    std::cout << "For production use, obtain a certificate from a trusted Certificate Authority.\n";

    return 0;
}
