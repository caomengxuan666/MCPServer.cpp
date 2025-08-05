// plugins/sdk/mcp_plugin.h
#pragma once

// Tool information provided by plugin
struct ToolInfo {
    const char *name;
    const char *description;
    const char *parameters;// JSON Schema string
    bool is_streaming = false;
};

// Function pointer types
using get_tools_func = ToolInfo *(*) (int *count);
using call_tool_func = const char *(*) (const char *name, const char *args_json);
using free_result_func = void (*)(const char *);


// streaming support
typedef void *StreamGenerator;

// if result_json is NULL, it means no more data
// return 0: more data
// return 1: end of stream
// return -1: error occurred
typedef int (*StreamGeneratorNext)(StreamGenerator generator, const char **result_json);

typedef void (*StreamGeneratorFree)(StreamGenerator generator);

// Function pointer types for streaming
using get_stream_next_func = StreamGeneratorNext (*)();
using get_stream_free_func = StreamGeneratorFree (*)();

struct StreamingResult {
    StreamGenerator generator;// the generator object
    StreamGeneratorNext next; // next function to get next result
    StreamGeneratorFree free; // the function to free the generator
};

//if the tool is synchronous, return a JSON string
//if the tool is streaming, return a StreamGenerator pointer (type erased)
extern "C" typedef const char *(*call_tool_func)(const char *name, const char *args_json);