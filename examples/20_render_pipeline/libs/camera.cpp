#include <cassert>

#include "tooling.h"
#include "camera.h"
#include "GLFW/glfw3.h"

using namespace nasl;

mat4 camera_rotation_matrix(const Camera* camera) {
    mat4 matrix = identity_mat4;
    matrix = mul_mat4(rotate_axis_mat4(1, camera->rotation.yaw), matrix);
    matrix = mul_mat4(rotate_axis_mat4(0, -camera->rotation.pitch), matrix);
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
    vec4 result = mul_mat4_vec4f(matrix, initial_forward);
    return vec3_scale(result.xyz, 1.0f / result.w);
}

vec3 camera_get_left_vec(const Camera* cam) {
    vec4 initial_forward(-1, 0, 0, 1);
    mat4 matrix = invert_mat4(camera_rotation_matrix(cam));
    vec4 result = mul_mat4_vec4f(matrix, initial_forward);
    return vec3_scale(result.xyz, 1.0f / result.w);
}


void camera_update(GLFWwindow* handle, CameraInput* input) {
    input->mouse_held = glfwGetMouseButton(handle, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    glfwGetCursorPos(handle, &input->mouse_x, &input->mouse_y);

    input->keys.forward = glfwGetKey(handle, GLFW_KEY_W) == GLFW_PRESS;
    input->keys.back = glfwGetKey(handle, GLFW_KEY_S) == GLFW_PRESS;
    input->keys.left = glfwGetKey(handle, GLFW_KEY_A) == GLFW_PRESS;
    input->keys.right = glfwGetKey(handle, GLFW_KEY_D) == GLFW_PRESS;
    input->keys.up = glfwGetKey(handle, GLFW_KEY_Q) == GLFW_PRESS;
    input->keys.down = glfwGetKey(handle, GLFW_KEY_Z) == GLFW_PRESS;

    if (input->should_capture)
        glfwSetInputMode(handle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    else
        glfwSetInputMode(handle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

#define THRESHOLD (4000.0 / 32768.0)
#define VALID(x) (x <= -THRESHOLD || x >= THRESHOLD)
#define CONVERT(x) (((x < 0) ? (x + THRESHOLD) : (x - THRESHOLD)) / (1.0 - THRESHOLD))

bool camera_move_freelook(Camera* cam, CameraInput* input, CameraFreelookState* state, float delta, struct STATE_UPDATE update) {
    assert(cam && input && state);
    bool moved = false;

    //Mouse and keyboard input
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
        cam->position = vec3_sub(cam->position, vec3_scale(camera_get_left_vec(cam), state->fly_speed * delta));
        moved = true;
    } else if (input->keys.left) {
        cam->position = vec3_add(cam->position, vec3_scale(camera_get_left_vec(cam), state->fly_speed * delta));
        moved = true;
    }

    if (input->keys.up) {
        cam->position = vec3_sub(cam->position, vec3_scale({0, 1, 0}, state->fly_speed * delta));
        moved = true;
    } else if (input->keys.down) {
        cam->position = vec3_add(cam->position, vec3_scale({0, 1, 0}, state->fly_speed * delta));
        moved = true;
    }

    bool l2 = false;
    bool r2 = false;

    //Controller input
    int present = glfwJoystickPresent(GLFW_JOYSTICK_1);
    if (present) {
        int count;
        const float* axes = glfwGetJoystickAxes(GLFW_JOYSTICK_1, &count);
        if (count >= 6) {
            float x_axis = axes[0];
            if (VALID(x_axis)) {
                cam->position = vec3_add(cam->position, vec3_scale(camera_get_left_vec(cam), -1.0 * CONVERT(x_axis) * state->fly_speed * delta));
                moved = true;
            }

            float y_axis = axes[1];
            if (VALID(y_axis)) {
                cam->position = vec3_add(cam->position, vec3_scale(camera_get_forward_vec(cam), -1.0 * CONVERT(y_axis) * state->fly_speed * delta));
                moved = true;
            }

            float vx_axis = axes[3];
            if (VALID(vx_axis)) {
                cam->rotation.yaw   += CONVERT(vx_axis) / (180.0f * (float) M_PI) * 6;
                moved = true;
            }

            float vy_axis = axes[4];
            if (VALID(vy_axis)) {
                cam->rotation.pitch += CONVERT(vy_axis) / (180.0f * (float) M_PI) * 6;
                moved = true;
            }

            float l2_axis = axes[2];
            if (l2_axis > -0.5)
                cam->position = vec3_add(cam->position, vec3_scale({0, 1, 0}, (l2_axis + 0.5) / 2 * state->fly_speed * delta));
            if (l2_axis > 0)
                l2 = true;
            float r2_axis = axes[5];
            if (r2_axis > -0.5)
                cam->position = vec3_sub(cam->position, vec3_scale({0, 1, 0}, (r2_axis + 0.5) / 2 * state->fly_speed * delta));
            if (r2_axis > 0)
                r2 = true;
        }

        const unsigned char* buttons = glfwGetJoystickButtons(GLFW_JOYSTICK_1, &count);
        if (count >= 15) {
            unsigned char a = buttons[0];
            unsigned char b = buttons[1];
            unsigned char x = buttons[2];
            unsigned char y = buttons[3];

            unsigned char l1 = buttons[4];
            unsigned char r1 = buttons[5];

            unsigned char map = buttons[6];
            unsigned char menu = buttons[7];
            unsigned char xbox = buttons[8];

            unsigned char l3 = buttons[9];
            unsigned char r3 = buttons[10];

            unsigned char up = buttons[11];
            unsigned char right = buttons[12];
            unsigned char down = buttons[13];
            unsigned char left = buttons[14];

            /*if (a)
                printf("a\n");
            if (b)
                printf("b\n");
            if (x)
                printf("x\n");
            if (y)
                printf("y\n");
            if (l1)
                printf("l1\n");
            if (r1)
                printf("r1\n");
            if (r2)
                printf("r2\n");
            if (l2)
                printf("l2\n");
            if (l3)
                printf("l3\n");
            if (r3)
                printf("r3\n");
            if (menu)
                printf("menu\n");
            if (map)
                printf("map\n");
            if (xbox)
                printf("xbox\n");
            if (up)
                printf("up\n");
            if (down)
                printf("down\n");
            if (right)
                printf("right\n");
            if (left)
                printf("left\n");*/

            static bool a_pressed = false;
            if (a && !a_pressed) {
                switch (*update.render_mode) {
                case FILL: *update.render_mode = GRID; break;
                case GRID: *update.render_mode = FILL; break;
                default: *update.render_mode = FILL; break;
                }
                a_pressed = true;
            } else if (!a) {
                a_pressed = false;
            }

            static bool x_pressed = false;
            if (x && !x_pressed) {
                if (*update.fog_dropoff_lower == 1.0f) {
                    *update.fog_dropoff_lower = *update.fog_lower_old;
                    *update.fog_dropoff_upper = *update.fog_upper_old;
                    *update.fog_power = *update.fog_power_old;
                } else {
                    *update.fog_lower_old = *update.fog_dropoff_lower;
                    *update.fog_upper_old = *update.fog_dropoff_upper;
                    *update.fog_power_old = *update.fog_power;

                    *update.fog_dropoff_lower = 1.0f;
                    *update.fog_dropoff_upper = 1.0f;
                    *update.fog_power = 1;
                }
                x_pressed = true;
            } else if (!x) {
                x_pressed = false;
            }

            static bool l1_pressed = false;
            if (l1 && !l1_pressed) {
                *update.tess_factor -= int(*update.tess_factor / 10) + 1;

                l1_pressed = true;
            } else if (!l1) {
                l1_pressed = false;
            }

            static bool r1_pressed = false;
            if (r1 && !r1_pressed) {
                *update.tess_factor += int(*update.tess_factor / 10) + 1;

                r1_pressed = true;
            } else if (!r1) {
                r1_pressed = false;
            }

            static bool map_pressed = false;
            if (map && !map_pressed) {
                *update.update_tess = *update.update_tess ^ 1;

                map_pressed = true;
            } else if (!map) {
                map_pressed = false;
            }

            static bool menu_pressed = false;
            if (menu && !menu_pressed) {
                *update.toggle_flight = 1;

                menu_pressed = true;
            } else if (!menu) {
                menu_pressed = false;
            }

            static bool up_pressed = false;
            if (up && !up_pressed) {
                if (y) {
                    *update.fog_dropoff_upper += (1 - *update.fog_dropoff_upper) * 0.1f;
                } else if (b) {
                    *update.fog_power += 1;
                } else {
                    *update.fog_dropoff_lower += (1 - *update.fog_dropoff_lower) * 0.1f;
                }

                up_pressed = true;
            } else if (!up) {
                up_pressed = false;
            }

            static bool down_pressed = false;
            if (down && !down_pressed) {
                if (y) {
                    *update.fog_dropoff_upper -= (1 - *update.fog_dropoff_upper) * 0.1f;
                } else if (b) {
                    *update.fog_power -= 1;
                } else {
                    *update.fog_dropoff_lower -= (1 - *update.fog_dropoff_lower) * 0.1f;
                }

                down_pressed = true;
            } else if (!down) {
                down_pressed = false;
            }

            static bool left_pressed = false;
	    if (left && !left_pressed) {
	        *update.fog_dropoff_lower = 0.98;
	        *update.fog_dropoff_upper = 0.995;
	        *update.fog_power = 10;
	        *update.tess_factor = 25.0f;
		left_pressed = true;
	    } else if (!left) {
		left_pressed = false;
	    }


	    if (right && y && b && !x && !a) {
		throw std::runtime_error("Weird controller exit be weird!");
	    }
        }

    }

    return moved;
}
