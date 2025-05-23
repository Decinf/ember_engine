#pragma once

#include <SDL3/SDL.h>
#include <glad/gl.h>
#include <cglm/cglm.h>
#include <stdio.h>
#include "node.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#define VB_ATTRIB_POS_OFFSET 0
#define VB_ATTRIB_CLR_OFFSET 1
#define VB_ATTRIB_UV_OFFSET 2
#define VB_ATTRIB_NORMAL_OFFSET 3


#define VB_ATTRIB_POS_SIZE 3
#define VB_ATTRIB_CLR_SIZE 3
#define VB_ATTRIB_UV_SIZE 2
#define VB_ATTRIB_NORMAL_SIZE 3
#define VB_ATTRIB_SIZE_MAX 11

//__________________________________________________
// prim - unique sample of the primitive
//__________________________________________________

/*
The simplest primitive consisting of triangles. 
Can contain only one material (shader). 
To assemble a full model from several primitives, you will need to use nodes.
*/
typedef struct //primitive template
{
    bool use_vertex_colors; //takes 3 elements in buffer
    bool use_uv; //2 additional elements in the buffer
    float * vb; //local vertex buffer 
    uint32_t vb_len; //length of the original buffer (in bytes)

    uint32_t * eb; //local triangles buffer 
    uint32_t eb_len; //length of the original triangles buffer (in bytes)
    
    
    GLuint shader_prog; //the single primitive support only one shader program
} prim; 




void prim_load_primitive_cgltf(prim *out, const cgltf_primitive *primitive) {
    size_t num_vertices = 0;
    size_t num_indices = 0;
    size_t vertex_stride = VB_ATTRIB_SIZE_MAX;

    //check possible errors 
    if (primitive->attributes_count > 0) num_vertices = primitive->attributes[0].data->count;
    if (primitive->indices) num_indices = primitive->indices->count;

    
    out->vb_len = num_vertices * vertex_stride * sizeof(float);
    out->vb = (float*)malloc(out->vb_len);

    out->eb_len = num_indices * sizeof(uint32_t);
    out->eb = (uint32_t*)malloc(out->eb_len);



    //fill vbo with zeros
    memset(out->vb, 0, out->vb_len);

    // fill vbo with values from cgltf primitive
    for (size_t i = 0; i < num_vertices; i++) {
        float *vbo_ver = &out->vb[i * vertex_stride];

        //for each attribute in current vertex
        for (size_t j = 0; j < primitive->attributes_count; j++) {
            cgltf_attribute *attr = &primitive->attributes[j];
            cgltf_accessor *accessor = attr->data;

            int num_components = 1;
            if (accessor->type == cgltf_type_vec2) num_components = 2;
            else if (accessor->type == cgltf_type_vec3) num_components = 3;
            else if (accessor->type == cgltf_type_vec4) num_components = 4;

            float temp[4] = {0}; //max 4 values for attrib
            cgltf_accessor_read_float(accessor, i, temp, num_components);

            switch (attr->type) {
                case cgltf_attribute_type_position:
                    vbo_ver[0] = temp[0];
                    vbo_ver[1] = temp[1];
                    vbo_ver[2] = temp[2];
                    break;
                case cgltf_attribute_type_color:
                    vbo_ver[3] = temp[0];
                    vbo_ver[4] = temp[1];
                    vbo_ver[5] = temp[2];
                    break;
                case cgltf_attribute_type_texcoord:
                    vbo_ver[6] = temp[0];
                    vbo_ver[7] = temp[1];
                    break;
                case cgltf_attribute_type_normal:
                    vbo_ver[8] = temp[0];
                    vbo_ver[9] = temp[1];
                    vbo_ver[10] = temp[2];
                    break;
                default:
                    break;
            }
        }
    }

    // fill ebo
    if (primitive->indices) {
        for (size_t i = 0; i < num_indices; i++) {
            out->eb[i] = (uint32_t)cgltf_accessor_read_index(primitive->indices, i);
        }
    }
}


//__________________________________________________
// mesh - array of primitives
// on instancing, should be merged by one node.
//__________________________________________________

typedef struct{
    char * name;
    prim * primitives;
    
    // inital mesh transformation
    vec3 pos;
    vec3 scale;
    vec3 rot;
    
} mesh;


cgltf_data * _model_load_gltf(char * path){
    cgltf_data * data = NULL; 
    cgltf_options options = {0};
    
    cgltf_result res = cgltf_parse_file(&options,path,&data);
    if(res != cgltf_result_success) return NULL;

    res = cgltf_load_buffers(&options,data,path);
    if(res != cgltf_result_success) return NULL;

    res = cgltf_validate(data);
    if(res != cgltf_result_success) return NULL;

    return data;
}
/*
void model_init_gltf(vec * out, char * path){
    cgltf_data * data = _vmodel_load_gltf(path);
    if (!data) return;

    //vec_alloc(sizeof(vmodel*),)
    
    for(cgltf_size i=0; i<data->meshes_count; ++i){
        cgltf_mesh* mesh = &data->meshes[i];
        for(cgltf_size j=0; j<mesh->primitives_count; ++j){
            cgltf_primitive * pr = &mesh->primitives[j]; vmodel *vm;
            vmodel_load_primitive_cgltf(vm,pr);
            

        }   
    }
    
}
*/



//__________________________________________________
// prim_inst - instance of the primitive
//__________________________________________________
typedef struct //vertex primitive (instance)
{    
    prim * primitive; //reference to the original primitive

    /*
    If the primitive is created successfully, 
    it gets its own vertex buffer, independent of the unique primitive. 
    Several instances can share one place in buffer (same vb_start), for optimisation.
    */

    float * vb_start; //reference to the vertex buffer
    uint32_t vb_len; //length in the vertex buffer (in bytes)

    uint32_t * eb_start; //reference to the element buffer
    uint32_t eb_len; //length in the element buffer (in bytes)

    // mat4 transform; //primitive matrix
    vec3 pos;
    vec3 scale;
    vec3 rot;

    // reference to the parent node. child node will inherit all the transformations.
    node * parent; 

} prim_inst; 


void prim_inst_get_transform(prim_inst * pr, mat4 m){
    glm_mat4_identity(m);
    glm_euler_xyz(pr->rot,m);
    glm_scale(m,pr->scale); //not affected by rotation
    glm_translated(m,pr->pos); //not affected by scale rotation

    if(pr->parent != NULL && pr->parent->node_state!=NODE_STATE_NONE){
        mat4 parent_tr;
        node_get_transform(pr->parent,parent_tr);
        glm_mat4_mul(parent_tr,m,m);
    }
};

void prim_inst_def_trtansform(prim_inst * pr){
    pr->pos[0] = 0.0f; pr->pos[1] = 0.0f; pr->pos[2] = 0.0f;
    pr->rot[0] = 0.0f; pr->rot[1] = 0.0f; pr->rot[2] = 0.0f;
    pr->scale[0] = 1.0f; pr->scale[1] = 1.0f; pr->scale[2] = 1.0f;
}















//primitive used for debugging (vertices)
float _prim_test_rectangle_vertices[] = {
        /*pos                       colors          uv      normals */
        -0.5f, -0.5f, -0.5f,    1.2f, 0.2f, 0.2f,   0,0,    -0.5773,-0.5773,-0.5773, //0
        0.5f, -0.5f, -0.5f, 0.2f, 1.2f, 0.2f,       0,0,    0.5773,-0.5773,-0.5773, //1
        -0.5f, 0.5f, -0.5f, 0.2f, 0.2f, 1.2f,       0,0,    -0.5773,0.5773,-0.5773,//2
        0.5f, 0.5f, -0.5f, 1.2f, 0.2f, 0.2f,        0,0,    0.5773,0.5773,-0.5773, //3


        -0.5f, -0.5f, 0.5f, 0.2f, 0.2f, 0.2f,       0,0,    -0.5773,-0.5773,0.5773,//4
       0.5f, -0.5f, 0.5f, 0.2f, 0.2f, 0.2f,         0,0,    0.5773,-0.5773,0.5773,//5
        -0.5f, 0.5f, 0.5f, 0.2f, 0.2f, 0.2f,        0,0,    -0.5773,0.5773,0.5773, //6
        0.5f, 0.5f, 0.5f, 0.2f, 0.2f, 0.2f,         0,0,    0.5773,0.5773,0.5773, //7
};
__uint32_t _prim_test_rectangle_elements[] = {
    1,0,2,
    1,2,3, //fron

    0,5,4,
    0,1,5, //top

    7,3,2,
    7,2,6,

    1,3,7,
    1,7,5,

    0,4,2,
    2,4,6,

    4,5,6,
    6,5,7,
};
//primitive used for debugging
prim prim_test_rectangle(){
    prim m;
    
    m.vb = _prim_test_rectangle_vertices;
    m.eb = _prim_test_rectangle_elements;
    //m.transform   
    m.use_vertex_colors = true;
    m.vb_len = sizeof(_prim_test_rectangle_vertices);
    m.eb_len = sizeof(_prim_test_rectangle_elements);
    return m;
}
