#include <cassert>

#include "camera.h"
#include "GLFW/glfw3.h"

using namespace nasl;

mat4 camera_rotation_matrix(const Camera* camera) {
    mat4 matrix = identity_mat4;
    matrix = mul_mat4(rotate_axis_mat4(1, camera->rotation.yaw), matrix);
    matrix = mul_mat4(rotate_axis_mat4(0, camera->rotation.pitch), matrix);
    return matrix;
}

mat4 camera_get_view_mat4(const Camera* camera, size_t width, size_t height) {
    mat4 matrix = identity_mat4;
    matrix = mul_mat4(translate_mat4(vec3_neg(camera->position)), matrix);
    matrix = mul_mat4(camera_rotation_matrix(camera), matrix);
    float ratio = ((float) width) / ((float) height);
    matrix = mul_mat4(perspective_mat4(ratio, camera->fov, 0.1f, 1000.f), matrix);
    return matrix;
}

mat4 rotate_axis_mat4f(unsigned int axis, float f) {
    mat4 m = { 0 };
    m.elems.m33 = 1;

    unsigned int t = (axis + 2) % 3;
    unsigned int s = (axis + 1) % 3;

    m.rows[t].arr[t] =  cosf(f);
    m.rows[t].arr[s] = -sinf(f);
    m.rows[s].arr[t] =  sinf(f);
    m.rows[s].arr[s] =  cosf(f);

    // leave that unchanged
    m.rows[axis].arr[axis] = 1;

    return m;
}

vec3 camera_get_forward_vec(const Camera* cam, vec3 forward) {
    vec4 initial_forward(forward, 1);
    // we invert the rotation matrix and use the front vector from the camera space to get the one in world space
    mat4 matrix = invert_mat4(camera_rotation_matrix(cam));
    vec4 result = mul_mat4_vec4(matrix, initial_forward);
    return vec3_scale(result.xyz, 1.0f / result.w);
}

vec3 camera_get_right_vec(const Camera* cam) {
    vec4 initial_forward(1, 0, 0, 1);
    mat4 matrix = invert_mat4(camera_rotation_matrix(cam));
    vec4 result = mul_mat4_vec4(matrix, initial_forward);
    return vec3_scale(result.xyz, 1.0f / result.w);
}


void camera_update(GLFWwindow* handle, CameraInput* input) {
    input->mouse_held = glfwGetMouseButton(handle, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    glfwGetCursorPos(handle, &input->mouse_x, &input->mouse_y);
    input->keys.forward = glfwGetKey(handle, GLFW_KEY_W) == GLFW_PRESS;
    input->keys.back = glfwGetKey(handle, GLFW_KEY_S) == GLFW_PRESS;
    input->keys.left = glfwGetKey(handle, GLFW_KEY_A) == GLFW_PRESS;
    input->keys.right = glfwGetKey(handle, GLFW_KEY_D) == GLFW_PRESS;
    if (input->should_capture)
        glfwSetInputMode(handle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    else
        glfwSetInputMode(handle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

bool camera_move_freelook(Camera* cam, CameraInput* input, CameraFreelookState* state, float delta) {
    assert(cam && input && state);
    bool moved = false;
    if (input->mouse_held) {
        if (state->mouse_was_held) {
            double diff_x = input->mouse_x - state->last_mouse_x;
            double diff_y = input->mouse_y - state->last_mouse_y;
            cam->rotation.yaw   += (float) diff_x / (180.0f * (float) M_PI) * state->mouse_sensitivity;
            cam->rotation.pitch += (float) diff_y / (180.0f * (float) M_PI) * state->mouse_sensitivity;
            moved = true;
        } else
            input->should_capture = true;

        state->last_mouse_x = input->mouse_x;
        state->last_mouse_y = input->mouse_y;
    } else
        input->should_capture = false;
    state->mouse_was_held = input->mouse_held;

    if (input->keys.forward) {
        cam->position = vec3_add(cam->position, vec3_scale(camera_get_forward_vec(cam), state->fly_speed * delta));
        moved = true;
    } else if (input->keys.back) {
        cam->position = vec3_sub(cam->position, vec3_scale(camera_get_forward_vec(cam), state->fly_speed * delta));
        moved = true;
    }

    if (input->keys.right) {
        cam->position = vec3_add(cam->position, vec3_scale(camera_get_right_vec(cam), state->fly_speed * delta));
        moved = true;
    } else if (input->keys.left) {
        cam->position = vec3_sub(cam->position, vec3_scale(camera_get_right_vec(cam), state->fly_speed * delta));
        moved = true;
    }
    return moved;
}
