// plugins/sdk/mcp_plugin.h
#pragma once

// Tool information provided by plugin
struct ToolInfo {
    const char *name;
    const char *description;
    const char *parameters;// JSON Schema string
    bool is_streaming = false;
};

struct MCPError {
    int code;           // error code,0 stand for no error
    const char *message;// readable message
    const char *details;// details
    const char *source; // error sources,such as modules
};


// Function pointer to call a tool
//if the tool is synchronous, return a JSON string
//if the tool is streaming, return a StreamGenerator pointer (type erased)
typedef const char *(*call_tool_func)(const char *name, const char *args_json, MCPError *error);

// Function pointer to get tools
typedef ToolInfo *(*get_tools_func)(int *count);

// Function pointer to free result
typedef void (*free_result_func)(const char *);

// streaming support
typedef void *StreamGenerator;

// if result_json is NULL, it means no more data
// return 0: more data
// return 1: end of stream
// return -1: error occurred
typedef int (*StreamGeneratorNext)(StreamGenerator generator, const char **result_json, MCPError *error);

typedef void (*StreamGeneratorFree)(StreamGenerator generator);

// Function pointer types for streaming
using get_stream_next_func = StreamGeneratorNext (*)();
using get_stream_free_func = StreamGeneratorFree (*)();

struct StreamingResult {
    StreamGenerator generator;// the generator object
    StreamGeneratorNext next; // next function to get next result
    StreamGeneratorFree free; // the function to free the generator
};

//cross platform strdup
#ifdef _WIN32
#define strdup _strdup
#endif