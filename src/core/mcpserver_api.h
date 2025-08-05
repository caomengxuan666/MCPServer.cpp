#ifndef MCPSERVER_API_H

#ifdef MCPSERVER_API_EXPORTS
#define MCP_API __declspec(dllexport)
#else
#define MCP_API __declspec(dllimport)
#endif

#define MCPSERVER_API_H

#endif