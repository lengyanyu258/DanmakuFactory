#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <yyjson.h>

int main()
{
    const char *json = "{\"name\":\"Mash\",\"star\":4,\"hits\":[2,2,1,3]}";
    
    // Read JSON and get root
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    if (!doc)
    {
        return -1;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!root)
    {
        yyjson_doc_free(doc);
        return -1;
    }

    // Get root["name"]
    yyjson_val *name = yyjson_obj_get(root, "name");
    printf("name: %s\n", yyjson_get_str(name));
    printf("name length:%d\n", (int)yyjson_get_len(name));

    // Get root["star"]
    yyjson_val *star = yyjson_obj_get(root, "star");
    printf("star: %d\n", yyjson_get_int(star));

    // Get root["hits"], iterate over the array
    yyjson_val *hits = yyjson_obj_get(root, "hits");
    size_t      idx, size;
    yyjson_val *hit;
    yyjson_arr_foreach(hits, idx, size, hit)
    {
        printf("hit(%d/%d): %d\n", (int)idx + 1, (int)size, yyjson_get_int(hit));
    }

    // Free the doc
    yyjson_doc_free(doc);

    // All functions accept NULL input, and return NULL on error.
    return 0;
}