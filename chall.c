#define  __STDC_WANT_LIB_EXT1__ 1

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>
#include <math.h>
#include <unistd.h>
#include <string.h>
#include <locale.h>
#include <wchar.h>
#include <signal.h>
#include <ncurses.h>

// If in CALC_FLAG mode, the XOR constant to get the correct flag is written to frob.txt at the end
#define CALC_FLAG 0

// Pixels into 2x2 sub pixels (can't be disabled)
#define ANTIALIASING 1

#define SCREEN_WIDTH 150
#define SCREEN_HEIGHT 60

#define WIDTH (SCREEN_WIDTH * (ANTIALIASING + 1))
#define HEIGHT (SCREEN_HEIGHT * (ANTIALIASING + 1))

#define MAZE_WIDTH 8
#define MAZE_HEIGHT 8
#define N_MAZES 11

#define mvprintwstr(x, y, fmt, ...) { \
    wchar_t line[1000]; \
    swprintf(line, 1000, fmt, __VA_ARGS__); \
    mvaddwstr(x, y, line); \
}

const char* corctf_s = 
    "                          @     @@    @                                                                           @  "\
    "                          @    @     @                                                                             @ "\
    " @@@   @@@  @ @@   @@@    @    @     @                                                                             @ "\
    "@   @ @   @ @@  @ @   @  @@@  @@@@  @                                                                               @"\
    "@     @   @ @     @       @    @     @                                                                             @ "\
    "@   @ @   @ @     @   @   @    @     @                                                                             @ "\
    " @@@   @@@  @      @@@     @   @      @                                                                           @  ";

#define FLAG_OFFS_X 41
#define FLAG_W 117
#define INNER_FLAG_W 71
#define FLAG_H 7

#define FLAG_TOP_Y 10
#define INNER_FLAG_SZ (INNER_FLAG_W * FLAG_H)

#if CALC_FLAG
const char* flag =
    " @@@        @   @  @@@  @@@@@   @   @   @  @@@@           @  @@@  @@@@ "\
    "@   @       @@ @@ @   @     @  @@   @@  @ @               @ @   @ @   @"\
    "@@@@@       @ @ @ @@@@@     @   @   @ @ @ @  @@           @ @   @ @@@@ "\
    "@   @  @@@  @   @ @   @    @    @   @  @@ @   @           @ @   @ @   @"\
    "@   @       @   @ @   @   @     @   @   @ @   @           @ @   @ @   @"\
    "@   @       @   @ @   @  @      @   @   @ @   @       @   @ @   @ @   @"\
    "@   @       @   @ @   @ @@@@@  @@@  @   @  @@@  @@@@@  @@@   @@@  @@@@ ";
   /*<--------------------------------------------------------------------->*/
uint8_t frob[INNER_FLAG_SZ] = {
};
#else
char flag[INNER_FLAG_SZ];
uint8_t frob[INNER_FLAG_SZ] = {
#include "frob.txt"
};
#endif  // CALC_FLAG

#define PLAYER_X_MIN 0.2
#define PLAYER_X_MAX 0.2
#define PLAYER_Y 0.1

#define PLAYER_MOVE_DELAY 4
#define KILL_PLANE -10.0


double camera_rad_per_frame = 0.01;

typedef struct color {
    uint8_t v;
} color;

typedef struct vec2 {
    float x;
    float y;
} vec2;

typedef struct vec3 {
    float x;
    float y;
    float z;
} vec3;

typedef struct mat3 {
    float e[9];
} mat3;

int on_the_move = 0;
vec3 player_target;
int player_fell_off = 0;
int center_camera_on_player = 0;
int game_over = 0;
int won = 0;
int touched_bad_square = 0;

uint8_t touched[MAZE_WIDTH * MAZE_HEIGHT * (N_MAZES + 1)];

float depth_buffer[HEIGHT][WIDTH];
color color_buffer[HEIGHT][WIDTH];

void print_mat3(mat3* a) {
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            wprintf(L"%f ", a->e[i * 3 + j]);
        }
        wprintf(L"\n");
    }
}

void print_vec2(vec2 b) {
    wprintf(L"Vec2(%f %f)\n", b.x, b.y);
}

void print_vec3(vec3 b) {
    wprintf(L"Vec3(%f %f %f)\n", b.x, b.y, b.z);
}

#define GEN_MUL(n)  \
    for (int i = 0; i < n; ++i) { \
        for (int j = 0; j < n; ++j) { \
            float sum = 0.0; \
            for (int k = 0; k < n; ++k) { \
                sum += a->e[i * n + k] * b->e[k * n + j]; \
            } \
            c->e[i * n + j] = sum; \
        } \
    }

void mat3_mul(const mat3* a, const mat3* b, mat3* c) {
    GEN_MUL(3)
}

void mat3_mul_vec3(const mat3* a, vec3 b, vec3* result) {
    float bf[3], bc[3];
    memcpy(bf, &b, sizeof(b));

    for (int i = 0; i < 3; ++i) {
        float sum = 0.0;
        for (int j = 0; j < 3; ++j) {
            sum += a->e[i * 3 + j] * bf[j];
        }
        bc[i] = sum;
    }

    memcpy(result, bc, sizeof(*result));
}


void mat3_add(const mat3* a, const mat3* b, mat3* c) {
    for (int i = 0; i < 9; ++i)
        c->e[i] = a->e[i] + b->e[i];
}

void mat3_sub(const mat3* a, const mat3* b, mat3* c) {
    for (int i = 0; i < 9; ++i)
        c->e[i] = a->e[i] - b->e[i];
}

void vec3_add(vec3 a, vec3 b, vec3* result) {
    result->x = a.x + b.x;
    result->y = a.y + b.y;
    result->z = a.z + b.z;
}

void vec3_sub(vec3 a, vec3 b, vec3* result) {
    result->x = a.x - b.x;
    result->y = a.y - b.y;
    result->z = a.z - b.z;
}

void vec3_scale(vec3 a, float sc, vec3* result) {
    result->x = a.x * sc;
    result->y = a.y * sc;
    result->z = a.z * sc;
}

void clear_draw() {
    memset(color_buffer, 0, sizeof(color_buffer));

    for (int i = 0; i < HEIGHT * WIDTH; ++i) {
        ((float*)depth_buffer)[i] = 1 / 0.0;
    }
}

const wchar_t* antialiasing_map = L" \0\u2598\0\u259d\0\u2580\0\u2596\0\u258c\0\u259e\0\u259b\0\u2597\0\u259a\0\u2590\0\u259c\0\u2584\0\u2599\0\u259f\0\u2588";

#define swap(a, b) { int tmp = a; a = b; b = tmp; }
void mode4(int a, int b, int c, int d, int* fg, int* bg, int* fg_bits) { 
	int _a = a, _b = b, _c = c, _d = d;

    if (a > b) swap(a, b);
    if (a > c) swap(a, c);
    if (a > d) swap(a, d);
    if (b > c) swap(b, c);
    if (b > d) swap(b, d);
    if (c > d) swap(c, d);

	int mode = d, other = 0;
    if (c == d && c != 0) {
			mode = c;
			other = a;
	} else if (b == c && b != 0) {
			mode = b;
			other = d;
	} else if (a == b && a != 0) {
			mode = a;
			other = c;
	}


	if (a == 0) {
		*bg = -1;
		*fg = mode;
		*fg_bits = ((!!_a) | (!!_b << 1) | (!!_c << 2) | (!!_d << 3));
	} else {
		*fg = mode;
		*fg_bits = ((_a == mode) | ((_b == mode) << 1) | ((_c == mode) << 2) | ((_d == mode) << 3));
		*bg = other;
	}
}
#undef swap

void draw() {
#define INIT_BY_C(f, b) init_pair(f * 16 + b + 1, f, b); 

    int colors[5] = { COLOR_RED, COLOR_WHITE, COLOR_BLUE, COLOR_YELLOW, COLOR_GREEN };

    for (int i = 0; i < 5; ++i) {
		INIT_BY_C(colors[i], -1);
		for (int j = 0; j < 5; ++j) {
			INIT_BY_C(colors[i], colors[j]);
		}
	}

    attron(COLOR_PAIR(COLOR_WHITE * 16));

    for (int i = 0; i < HEIGHT; i += 2) {
        for (int j = 0; j < WIDTH; j += 2) {
            int c1 = color_buffer[i][j].v;
            int c2 = color_buffer[i][j + 1].v;
            int c3 = color_buffer[i + 1][j].v;
            int c4 = color_buffer[i + 1][j + 1].v;

			int fg, bg, map_bits;

            mode4(c1, c2, c3, c4, &fg, &bg, &map_bits);
			attron(COLOR_PAIR(fg * 16 + bg + 1));

            mvaddwstr(i / 2, j / 2, &antialiasing_map[2 * map_bits]);
			attron(COLOR_PAIR(COLOR_WHITE * 16));
        }
    }

    attroff(COLOR_PAIR(COLOR_WHITE * 16));
}

void mat3_from_euler_angles(float xt, float yt, float zt, mat3* result) {
    float a[9] = { 1, 0, 0, 0, cos(xt), sin(xt), 0, -sin(xt), cos(xt) };
    float b[9] = { cos(yt), 0, -sin(yt), 0, 1, 0, sin(yt), 0, cos(yt) };
    float c[9] = { cos(zt), sin(zt), 0, -sin(zt), cos(zt), 0, 0, 0, 1 };

    mat3 r1;

    mat3_mul((mat3*) a, (mat3*) b, &r1);
    mat3_mul(&r1, (mat3*) c, result); 
}

typedef struct camera {
    vec3 draw_plane;
    vec3 position;

    mat3 camera_rotation;

    float xt;
    float yt;
    float zt;
} camera;

void camera_set_rotation(camera* c, float xt, float yt, float zt) {
    mat3_from_euler_angles(xt, yt, zt, &c->camera_rotation);

    c->xt = xt;
    c->yt = yt;
    c->zt = zt;    
}

void init_camera(camera* c) {
    c->position = (vec3) { .x = 0, .y = 0, .z = 0 };
    c->draw_plane = (vec3) { .x = 0, .y = 0, .z = 1.25 };
    
    camera_set_rotation(c, 0, 0, 0);
}

vec2 camera_project_to_ndc(const camera* c, const vec3 point, int* wrong_side, vec3* rotated_result) {
    // See https://en.wikipedia.org/wiki/3D_projection#Perspective_projection
    vec3 disp, rotated;
    vec3_sub(point, c->position, &disp);

    mat3_mul_vec3(&c->camera_rotation, disp, &rotated);

    float ez_rcp = c->draw_plane.z / rotated.z;
    float x = c->draw_plane.x + ez_rcp * rotated.x;
    float y = c->draw_plane.y + ez_rcp * rotated.y;

    *wrong_side = *wrong_side || (rotated.z < c->draw_plane.z);  // sticky
    if (rotated_result) {
        memcpy(rotated_result, &rotated, sizeof(vec3));
    }

    vec2 result = { .x = x, .y = y };  
    return result;
}

vec2 scale_ndc_to_screen(const vec2 a, int* in_bounds) {
    float x = 0.0, y = 0.0;
    *in_bounds = !(a.x < -1.0 || a.x > 1.0 || a.y < -1.0 || a.y > 1.0);

    x = (a.x + 1.0) * (WIDTH / 2);
    y = (a.y + 1.0) * (HEIGHT / 2);

    return (vec2) { .x = x, .y = y };
}

typedef struct colored_point {
    int color;
    vec3 pos;
} colored_point;

void draw_pixel(int x, int y, float depth, int color) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;

    if (depth_buffer[y][x] >= depth) {
        depth_buffer[y][x] = depth;

        color_buffer[y][x].v = color;
    }

}

void draw_point(const camera* c, const colored_point pt) {
    int in_front;
    vec3 rotated;   // camera space
    vec2 t = camera_project_to_ndc(c, pt.pos, &in_front, &rotated);

    int in_bounds;
    vec2 screen_c = scale_ndc_to_screen(t, &in_bounds);

    if (!in_bounds || !in_front) return;

    draw_pixel(screen_c.x, screen_c.y, rotated.z, pt.color);
}

typedef float bary_coord[3];

void vec2_add(vec2 a, vec2 b, vec2* res) {
    res->x = a.x + b.x;
    res->y = a.y + b.y;
}

void vec2_sub(vec2 a, vec2 b, vec2* res) {
    res->x = a.x - b.x;
    res->y = a.y - b.y;
}

float vec2_dot(vec2 a, vec2 b) {
    return a.x * b.x + a.y * b.y;
}

struct barycentric_context {
    vec2 a, b, c, v0, v1;
    float d00, d01, d11, rcp_d;
};

//gamedev.stackexchange.com/a/23745tx->d01 = vec2_dot(a, b);
void init_barycentric_context(struct barycentric_context* ctx, vec2 a, vec2 b, vec2 c) {
    ctx->a = a;
    ctx->b = b;
    ctx->c = c;

    vec2_sub(b, a, &ctx->v0);
    vec2_sub(c, a, &ctx->v1);

    ctx->d00 = vec2_dot(ctx->v0, ctx->v0);
    ctx->d01 = vec2_dot(ctx->v0, ctx->v1);
    ctx->d11 = vec2_dot(ctx->v1, ctx->v1);
    ctx->rcp_d = 1.0 / (ctx->d00 * ctx->d11 - ctx->d01 * ctx->d01);
}

// https://realtimecollisiondetection.net/
void barycentric(vec2 p, const struct barycentric_context* ctx, bary_coord coords) {
    vec2 v2;
    float d20, d21;

    vec2_sub(p, ctx->a, &v2);

    d20 = vec2_dot(v2, ctx->v0);
    d21 = vec2_dot(v2, ctx->v1);

    coords[0] = (ctx->d11 * d20 - ctx->d01 * d21) * ctx->rcp_d;
    coords[1] = (ctx->d00 * d21 - ctx->d01 * d20) * ctx->rcp_d;
    coords[2] = 1.0 - coords[0] - coords[1];
}

int int_min(int a, int b) {
    return (a < b) ? a : b;
}

int int_max(int a, int b) {
    return (a > b) ? a : b;
}

typedef struct triangle3 {
    vec3 a;
    vec3 b;
    vec3 c;
    int color;
} triangle3;

void draw_triangle(const camera* cam, const triangle3* tri) {
    vec3 a = tri->a, b = tri->b, c = tri->c;

    vec2 at, bt, ct;
    vec3 a_rot, b_rot, c_rot;

    int wrong_side = 0;

    at = camera_project_to_ndc(cam, a, &wrong_side, &a_rot);
    bt = camera_project_to_ndc(cam, b,  &wrong_side, &b_rot);
    ct = camera_project_to_ndc(cam, c,  &wrong_side, &c_rot);

    if (wrong_side) return;

    int _1, _2, _3;
    at = scale_ndc_to_screen(at, &_1);
    bt = scale_ndc_to_screen(bt, &_2);
    ct = scale_ndc_to_screen(ct, &_3);

    struct barycentric_context ctx;
    init_barycentric_context(&ctx, at, bt, ct);


    int min_x = int_max(int_min(int_min(at.x, bt.x), ct.x), 0);
    int min_y = int_max(int_min(int_min(at.y, bt.y), ct.y), 0);
    int max_x = int_min(int_max(int_max(at.x, bt.x), ct.x), WIDTH - 1);
    int max_y = int_min(int_max(int_max(at.y, bt.y), ct.y), HEIGHT - 1);


    bary_coord bcs;
    float a_rotz_rcp = 1.0 / a_rot.z;
    float b_rotz_rcp = 1.0 / b_rot.z;
    float c_rotz_rcp = 1.0 / c_rot.z;

    for (int x = min_x; x <= max_x; ++x) {
        for (int y = min_y; y <= max_y; ++y) {
            barycentric((vec2) { .x = x, .y = y }, &ctx, bcs);

            if (bcs[0] < 0 || bcs[0] > 1 || bcs[1] < 0 || bcs[1] > 1 || bcs[2] < 0 || bcs[2] > 1)
                continue;
            if (!(_1 || _2 || _3)) continue;

            float depth = bcs[2] * a_rotz_rcp + bcs[0] * b_rotz_rcp + bcs[1] * c_rotz_rcp;
            depth = 1.0 / depth;

            if (depth > cam->draw_plane.z) {
                draw_pixel(x, y, depth, tri->color);
            }
        }
    }
}

volatile sig_atomic_t exited = 0;
void exit_handler() {
    if (exited) return;
    exited = 1;
}

typedef enum __attribute__((__packed__)) {
    MAZE_EMPTY = 0,   // ' ' player falls off (square not drawn)
    MAZE_AVOID = 1,   // 'x' player cannot touch this square
    MAZE_GOOD = 2,    // '@' player can touch this square (drawn same as AVOID)
    MAZE_START = 3,   // 's' player starts on this square. If the player moves onto it, it's not allowed
    MAZE_END = 4,      // 'e' player passes to next level on this square
    MAZE_EPHEMERAL = 5,   // can only be stepped on once

    MAZE_TRIGGER_1 = 6,  // when stepped, toggles all squares MAZE_TRIGGERED_1 or MAZE_HIDDEN_1
    MAZE_TRIGGERED_1 = 7,
    MAZE_HIDDEN_1 = 8,
    MAZE_TRIGGER_2 = 9,
    MAZE_TRIGGERED_2 = 10,
    MAZE_HIDDEN_2 = 11,

    MAZE_LEVITATE = 12   // square can be walked on but does not show
} maze_square;

typedef struct maze {
    int width;
    int height;
    int squares;

    maze_square* maze; 
    triangle3* triangles;

    int triangles_count;
    int triangles_alloc;

    vec3 player_pos;
    int player_visible;

} maze;

const char* maze_init[N_MAZES] = {
#include "mazes.h"
};

maze* m;     // active maze
int maze_i;  // index of active maze

maze_square m_encoding[] = {
    ['@'] = MAZE_GOOD,
    [' '] = MAZE_EMPTY,
    ['x'] = MAZE_AVOID,
    ['L'] = MAZE_LEVITATE,
    ['1'] = MAZE_TRIGGER_1,
    ['2'] = MAZE_TRIGGER_2,
    ['A'] = MAZE_TRIGGERED_1,
    ['B'] = MAZE_TRIGGERED_2,
    ['a'] = MAZE_HIDDEN_1,
    ['b'] = MAZE_HIDDEN_2,
    ['s'] = MAZE_START,
    ['e'] = MAZE_END,
    ['#'] = MAZE_EPHEMERAL
};

void maze_set_squares(maze* m, const char* s) {
    int i = 0;

    while (*s && i < m->squares) {
        char c = *s++;
        if (c == 'n') continue;

        m->maze[i++] = m_encoding[(int)c];
    }
}

maze_square maze_square_at(maze* m, int x, int y) {
    assert(x >= 0 && x < m->width);
    assert(y >= 0 && y < m->height);

    return m->maze[y * m->width + x];
}

maze* init_maze() {
    int width = MAZE_WIDTH, height = MAZE_HEIGHT;

    maze* m = malloc(sizeof(maze)); 
    m->width = width;
    m->height = height;

    m->squares = width * height;
    m->maze = calloc(m->squares, sizeof(maze_square));

    m->triangles = NULL;
    m->triangles_count = m->triangles_alloc = 0; 

    maze_set_squares(m, maze_init[maze_i]);

    int i, j;

    for (i = 0; i < MAZE_WIDTH; ++i) {
        for(j = 0; j < MAZE_HEIGHT; ++j) {
            if (maze_square_at(m, i, j) == MAZE_START) {
                goto a;
            }
        }
    }
a:

    m->player_pos = (vec3) { .x = i, .y = 0, .z = j };
    m->player_visible = 1;

    return m;
}


void maze_set_square(maze* m, int x, int y, maze_square s) {
    assert(x >= 0 && x < m->width);
    assert(y >= 0 && y < m->height);

    m->maze[y * m->width + x] = s;
}

void free_maze(maze* m) {
    free(m->triangles);
    free(m->maze);
    free(m);
}

void maze_fit_triangles(maze* m, int count) {
    if (count == 0) count = 1;
    if (count >= m->triangles_alloc) {
        count = (1 << (33 - __builtin_clzl(count)));

        m->triangles = realloc(m->triangles, count * sizeof(triangle3));
        m->triangles_alloc = count;
    }
}

void maze_push_triangle(maze* m, const triangle3* tri) {
    int count = m->triangles_count;

    maze_fit_triangles(m, count);

    memcpy(&m->triangles[count], tri, sizeof(triangle3));
    m->triangles_count++;
}

int is_visible_square(maze_square s) {
    return s == MAZE_GOOD || s == MAZE_AVOID || s == MAZE_TRIGGER_1 || s == MAZE_TRIGGER_2 || s == MAZE_TRIGGERED_1 || s == MAZE_TRIGGERED_2 || s == MAZE_EPHEMERAL
        || s == MAZE_START || s == MAZE_END;
}

int square_color(maze_square s) {
    switch (s) {
        case MAZE_END: return COLOR_GREEN;
        case MAZE_START: return COLOR_YELLOW;
        default:
                         return -1;
    }
}

void maze_compute_triangles(maze* m) { 
    m->triangles_count = 0;

    for (int i = 0; i < m->width; ++i) {
        for (int j = 0; j < m->height; ++j) {
            maze_square s = maze_square_at(m, i, j);
            if (is_visible_square(s)) {

                int checkerboard = ((i + j) % 2) ? COLOR_RED : COLOR_BLUE; 

                int c = square_color(s);
                if (c == -1)
					c = checkerboard;

				triangle3 tri = (triangle3) {
					.color = c,
					.a = (vec3) { .x = i, .y = 0, .z = j },            
					.b = (vec3) { .x = i + 1, .y = 0, .z = j },            
					.c = (vec3) { .x = i, .y = 0, .z = j + 1 },            
				};
				maze_push_triangle(m, &tri);

				tri = (triangle3) {
					.color = c,
					.a = (vec3) { .x = i + 1, .y = 0, .z = j },            
					.b = (vec3) { .x = i, .y = 0, .z = j + 1 },            
					.c = (vec3) { .x = i + 1, .y = 0, .z = j + 1 },            
				};
				maze_push_triangle(m, &tri);
			}
        }
    }
}

void maze_make_flag(maze* m, float size) {
    for (int x = 0; x < FLAG_W; ++x) {
        int k = x - FLAG_OFFS_X;
        for (int y = 0; y < FLAG_H; ++y) {
            if ((k < INNER_FLAG_W && k >= 0) ? (flag[y * INNER_FLAG_W + k] == '@') : (corctf_s[y * FLAG_W + x] == '@'))  {
                float xb = (x + MAZE_WIDTH / 2 - FLAG_W / 2.0) * size, yb = FLAG_TOP_Y - y * size, flag_z = MAZE_HEIGHT / 2;

                triangle3 tri = (triangle3) {
                    .color = COLOR_GREEN,
                    .a = (vec3) { .x = xb, .y = yb, .z = flag_z },
                    .b = (vec3) { .x = xb + size, .y = yb, .z = flag_z },
                    .c = (vec3) { .x = xb + size, .y = yb - size, .z = flag_z }
                };
                maze_push_triangle(m, &tri);
                tri = (triangle3) {
                    .color = COLOR_GREEN,
                        .a = (vec3) { .x = xb, .y = yb, .z = flag_z },
                        .b = (vec3) { .x = xb, .y = yb - size, .z = flag_z },
                        .c = (vec3) { .x = xb + size, .y = yb - size, .z = flag_z }
                };
                maze_push_triangle(m, &tri);
            }
        }
    }
}

triangle3* player_model;
int player_triangle_count;

void translate_tri3(const triangle3* tri, vec3 tr, triangle3* result) {
    vec3_add(tri->a, tr, &result->a);
    vec3_add(tri->b, tr, &result->b);
    vec3_add(tri->c, tr, &result->c);

    result->color = tri->color;
}

void render_maze(maze* m, const camera* c) {
    maze_compute_triangles(m);
    if (won) maze_make_flag(m, 0.3);

    for (int i = 0; i < player_triangle_count; ++i) {
        triangle3 a;
        translate_tri3(&player_model[i], m->player_pos, &a);

        maze_push_triangle(m, &a);
    }

    for (int i = 0; i < m->triangles_count; ++i) {
        draw_triangle(c, &m->triangles[i]);
    }

}

void make_player_model() {
    player_model = malloc(12 * sizeof(triangle3));

    player_model[0] = (triangle3) {
        .a = (vec3) { .x = 0.2, .y = 0, .z = 0.2 },
        .b = (vec3) { .x = 0.8, .y = 0, .z = 0.2 },
        .c = (vec3) { .x = 0.8, .y = 0, .z = 0.8 },
        .color = COLOR_YELLOW
    };
    player_model[1] = (triangle3) {
        .a = (vec3) { .x = 0.2, .y = 0, .z = 0.2 },
        .b = (vec3) { .x = 0.8, .y = 0, .z = 0.8 },
        .c = (vec3) { .x = 0.2, .y = 0, .z = 0.8 },
        .color = COLOR_YELLOW
    };
    player_model[2] = (triangle3) {
        .a = (vec3) { .x = 0.2, .y = 0, .z = 0.2 },
        .b = (vec3) { .x = 0.8, .y = 0, .z = 0.2 },
        .c = (vec3) { .x = 0.5, .y=2.4, .z = 0.5 },
        .color = COLOR_YELLOW
    };
    player_model[3] = (triangle3) {
        .a = (vec3) { .x = 0.8, .y = 0, .z = 0.2 },
        .b = (vec3) { .x = 0.8, .y = 0, .z = 0.8 },
        .c = (vec3) { .x = 0.5, .y=2.4, .z = 0.5 },
        .color = COLOR_YELLOW
    };
    player_model[4] = (triangle3) {
        .a = (vec3) { .x = 0.8, .y = 0, .z = 0.8 },
        .b = (vec3) { .x = 0.2, .y = 0, .z = 0.8 },
        .c = (vec3) { .x = 0.5, .y=2.4, .z = 0.5 },
        .color = COLOR_YELLOW
    };
    player_model[5] = (triangle3) {
        .a = (vec3) { .x = 0.2, .y = 0, .z = 0.8 },
        .b = (vec3) { .x = 0.2, .y = 0, .z = 0.2 },
        .c = (vec3) { .x = 0.5, .y=2.4, .z = 0.5 },
        .color = COLOR_YELLOW
    };

    player_triangle_count = 6;
}

camera c;

void clear_touched() {
    memset(touched, 0, sizeof(touched));
}

uint64_t hash_touched() {
    uint64_t m = 0;

    for (int i = 0; i < sizeof(touched); ++i) {
        m += touched[i];
        m *= 0x314159265;
        m = (m >> 21) | (m << 43);
    }

    return m;
}

void get_flag() {
    uint64_t h = hash_touched();

    for (int i = 0; i < INNER_FLAG_SZ; ++i) {
        h += 0x102;
        h *= 0x314159265;
        h = (h >> 22) | (h << 42);

#if CALC_FLAG
        frob[i] = flag[i] ^ (h & 0xff);
#else
        flag[i] = frob[i] ^ (h & 0xff);
#endif
    }

#if CALC_FLAG
    endwin();

    FILE* f = fopen("frob.txt", "w");

    for (int i = 0; i < INNER_FLAG_SZ; ++i) {
        fprintf(f, "%d, ", frob[i]);
    }

    fclose(f);
    exited = 1;
#endif
}

void do_win() {
    center_camera_on_player = 0;
    won = 1;
    get_flag();
}

int center_s(const wchar_t* s) {
    return SCREEN_WIDTH / 2 - wcslen(s) / 2;
}


void player_kill() {
    if (won) return;
    game_over = 1;

    if (player_fell_off) {
        const wchar_t* s1 = L"    You fell off :(    ";
        const wchar_t* s2 = L"     R - Try again     ";
        const wchar_t* s3 = L"                       ";

        int y;

        mvaddwstr(y = SCREEN_HEIGHT / 2 - 2, center_s(s3), s3);
        mvaddwstr(++y, center_s(s3), s3);
        mvaddwstr(++y, center_s(s1), s1);
        mvaddwstr(++y, center_s(s2), s2);
        mvaddwstr(++y, center_s(s3), s3);
        mvaddwstr(++y, center_s(s3), s3);
    }
}

int didnt_like_that = -1;

void next_maze() {
    touched_bad_square = 0;
    if (maze_i >= N_MAZES - 1) {
        do_win();
    } else {
        maze_i++;

        free_maze(m);
        m = init_maze();
    }
}

void prev_maze() {
    touched_bad_square = 0;
    if (maze_i != 0) {
        maze_i -= 1;
    }

    free_maze(m);
    m = init_maze();

    didnt_like_that = 20;
    memset(&touched[m->squares * maze_i], 0, m->squares);
}

void init();

void handle_input(int c) {
    vec3 attempted_move;
    int do_move = 0;

    switch (c) {
        case 'w':
            attempted_move = (vec3) { .x = 0.0, .y = 0.0, .z = 1.0 };
            do_move = 1;
            break;
        case 'a':
            attempted_move = (vec3) { .x = 1.0, .y = 0.0, .z = 0.0 };
            do_move = 1;
            break;
        case 's':
            attempted_move = (vec3) { .x = 0.0, .y = 0.0, .z = -1.0 };
            do_move = 1;
            break;
        case 'd':
            attempted_move = (vec3) { .x = -1.0, .y = 0.0, .z = 0.0 };
            do_move = 1;
            break;
        case 'c':
            center_camera_on_player ^= 1;
            break;
        case 'r':
            init();
            return;
        case 'q':
            exited = 1;
            return;
    }

    if (!game_over && !player_fell_off && on_the_move < 0 && do_move) {
        int cx = m->player_pos.x, cy = m->player_pos.z;
        maze_square current = maze_square_at(m, cx, cy);
        
        vec3_add(m->player_pos, attempted_move, &player_target);
        int x = player_target.x, z = player_target.z;
        if (x >= MAZE_WIDTH || z >= MAZE_HEIGHT || x < 0 || z < 0) {
            player_fell_off = 1;
        } else {
            maze_square sq = maze_square_at(m, x, z);

            switch(sq) {
                case MAZE_EMPTY:
                case MAZE_HIDDEN_1:
                case MAZE_HIDDEN_2:
                    player_fell_off = 1;
                    break;
                case MAZE_START:
                case MAZE_AVOID:
                    touched_bad_square = 1;
                    break;
                case MAZE_TRIGGERED_1:
                case MAZE_TRIGGERED_2:
                case MAZE_GOOD:
                case MAZE_LEVITATE:
                case MAZE_EPHEMERAL:
                    break;
                case MAZE_END: {
                    if (!touched_bad_square) {
                        next_maze();
                    } else {
                        prev_maze();
                    }
                    return;
                }
                case MAZE_TRIGGER_1:
                    for (int i = 0; i < m->squares; ++i) {
                        if (m->maze[i] == MAZE_TRIGGERED_1) {
                            m->maze[i] = MAZE_HIDDEN_1;
                        } else if (m->maze[i] == MAZE_HIDDEN_1) {
                            m->maze[i] = MAZE_TRIGGERED_1;
                        }
                    }
                    break;
                case MAZE_TRIGGER_2:
                    for (int i = 0; i < m->squares; ++i) {
                        if (m->maze[i] == MAZE_TRIGGERED_2) {
                            m->maze[i] = MAZE_HIDDEN_2;
                        } else if (m->maze[i] == MAZE_HIDDEN_2) {
                            m->maze[i] = MAZE_TRIGGERED_2;
                        }
                    }
                    break;
            }

            if (current == MAZE_EPHEMERAL) {
                maze_set_square(m, cx, cy, MAZE_EMPTY);
            }

            if (sq != MAZE_EPHEMERAL && sq != MAZE_END && sq != MAZE_START) {
                touched[m->squares * maze_i + z * MAZE_WIDTH + x] = 1;
            }
            
        }

        on_the_move = PLAYER_MOVE_DELAY - 1;
    }
}

float camera_rot = 3;
const float gravity_strength = 0.0025;

void tick() {
    camera_rot += camera_rad_per_frame;
    float focus_y = 0;
    float hi_tilt = 2.2;

    if (won) {
        focus_y = 5;
        hi_tilt = 2.7;
    }

    vec3 focus = (vec3) { MAZE_WIDTH / 2, focus_y, MAZE_HEIGHT / 2 };
    if (center_camera_on_player) {
        focus = m->player_pos;
        vec3_add(focus, (vec3) { .x = 0.5, .y = 0.0, .z = 0.5 }, &focus);
        camera_rot = M_PI;
    }

    vec3_add(focus, (vec3) { .x = 6 * sin(camera_rot), .y = 6, .z = 6 * cos(camera_rot) }, &c.position);
    camera_set_rotation(&c, hi_tilt, camera_rot, 0);

    clear_draw();
    render_maze(m, &c);
    draw();

    if (on_the_move >= 0) {
        vec3 disp;
        vec3_sub(player_target, m->player_pos, &disp);
        vec3_scale(disp, (PLAYER_MOVE_DELAY - on_the_move) * (1.0 / PLAYER_MOVE_DELAY), &disp);
        vec3_add(disp, m->player_pos, &m->player_pos);
    }

    if (didnt_like_that >= 0) {
        int y = SCREEN_HEIGHT / 2 - 2;
        const wchar_t* c1 = L"                         ";
        const wchar_t* c2 = L"     That's not right.   ";
        const wchar_t* c3 = L" Here's an easier level. ";
        mvaddwstr(y++, center_s(c1), c1);
        mvaddwstr(y++, center_s(c2), c2);
        mvaddwstr(y++, center_s(c3), c3);
        mvaddwstr(y++, center_s(c1), c1);
    }

    on_the_move--;
    didnt_like_that--;

    if (player_fell_off) {
        if (on_the_move < 0) {
            m->player_pos.y += on_the_move * gravity_strength;
        }

        if (m->player_pos.y < KILL_PLANE) {
            player_kill();
        }
    }

    refresh();
}

void init() {
    if (m) free_maze(m);
    maze_i = 0;
    clear_touched();

    won = 0;
    touched_bad_square = 0;
    camera_rot = 3;

    game_over = 0;
    center_camera_on_player = 0;
    player_fell_off = 0;
    on_the_move = -1;

    make_player_model();
    m = init_maze();
    init_camera(&c);

    m->player_visible = 1;

    init_maze();
}


int main() {
    atexit(exit_handler);
    signal(SIGINT, exit_handler);

    setlocale(LC_ALL, "");
    fwide(stdout, 1);

    initscr();
    use_default_colors();
    start_color();
    noecho();
    timeout(0);

    init();

    while (1) { 
        int inp = getch();
        if (inp != EOF) {
            handle_input(inp);
        }

        struct timespec start, end;
        usleep(30000);

        clock_gettime(CLOCK_REALTIME, &start);

        tick();

        clock_gettime(CLOCK_REALTIME, &end);

        float us_frame = ((end.tv_sec - start.tv_sec) * 1.0e6 + (end.tv_nsec - start.tv_nsec) * 1.0e-3);
        mvaddwstr(SCREEN_HEIGHT, 0, L"AMAZEING Renderer (TM) v0.1\nWASD - move\nC - change camera mode\nQ - quit\nR - restart");
        mvprintwstr(SCREEN_HEIGHT + 5, 0, L"us per frame: %f\n", us_frame);
        mvprintwstr(SCREEN_HEIGHT + 5, 0, L"Level %d / %d\n", maze_i + 1, N_MAZES);

        if (exited) {
            endwin();
            fflush(stdout);
            exit(0);
        }
    }
}
