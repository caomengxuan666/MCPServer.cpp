#ifndef MCPSERVER_API_H
#define MCPSERVER_API_H

#if defined(_WIN32) || defined(_WIN64)
#ifdef MCPSERVER_API_EXPORTS
#define MCP_API __declspec(dllexport)
#else
#define MCP_API __declspec(dllimport)
#endif
#else
#define MCP_API __attribute__((visibility("default")))
#endif

#endif// MCPSERVER_API_H