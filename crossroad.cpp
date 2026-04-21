#include "crossroad.h"
#include "hw2_output.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

static int  g_h_lanes, g_v_lanes;
static int *g_h_pri,  *g_v_pri;

static int *g_drv_h, *g_drv_v;

static long long *g_ticket_h,  *g_ticket_v;
static long long *g_served_h,  *g_served_v;

static long long *g_front_seq_h, *g_front_seq_v;
static long long  g_arrival_seq;

static pthread_mutex_t g_mutex;
static pthread_cond_t  g_cond;

static int any_h_driving(void) {
    for (int i = 0; i < g_h_lanes; i++)
        if (g_drv_h[i] > 0) return 1;
    return 0;
}

static int any_v_driving(void) {
    for (int i = 0; i < g_v_lanes; i++)
        if (g_drv_v[i] > 0) return 1;
    return 0;
}

static int max_crossing_priority(Direction dir) {
    int max_pri = -1;
    if (dir == DIR_HORIZONTAL) {
        for (int i = 0; i < g_v_lanes; i++)
            if (g_front_seq_v[i] >= 0 && g_v_pri[i] > max_pri)
                max_pri = g_v_pri[i];
    } else {
        for (int i = 0; i < g_h_lanes; i++)
            if (g_front_seq_h[i] >= 0 && g_h_pri[i] > max_pri)
                max_pri = g_h_pri[i];
    }
    return max_pri;
}

static long long earliest_crossing_seq_at_priority(Direction dir, int priority) {
    long long earliest = (long long)9e18;
    if (dir == DIR_HORIZONTAL) {
        for (int i = 0; i < g_v_lanes; i++)
            if (g_front_seq_v[i] >= 0 && g_v_pri[i] == priority && g_front_seq_v[i] < earliest)
                earliest = g_front_seq_v[i];
    } else {
        for (int i = 0; i < g_h_lanes; i++)
            if (g_front_seq_h[i] >= 0 && g_h_pri[i] == priority && g_front_seq_h[i] < earliest)
                earliest = g_front_seq_h[i];
    }
    return earliest;
}

static int can_enter(Direction dir, int lane, long long my_seq) {
    if (dir == DIR_HORIZONTAL) {
        if (any_v_driving()) return 0;
        if (g_drv_h[lane] > 0) return 0;
        int my_pri = g_h_pri[lane];
        int max_cross_pri = max_crossing_priority(DIR_HORIZONTAL);
        if (max_cross_pri > my_pri) return 0;
        if (max_cross_pri == my_pri) {
            if (earliest_crossing_seq_at_priority(DIR_HORIZONTAL, my_pri) < my_seq) return 0;
        }
    } else {
        if (any_h_driving()) return 0;
        if (g_drv_v[lane] > 0) return 0;
        int my_pri = g_v_pri[lane];
        int max_cross_pri = max_crossing_priority(DIR_VERTICAL);
        if (max_cross_pri > my_pri) return 0;
        if (max_cross_pri == my_pri) {
            if (earliest_crossing_seq_at_priority(DIR_VERTICAL, my_pri) < my_seq) return 0;
        }
    }
    return 1;
}

void initialize_crossroad(int h_lanes, int v_lanes, int* h_pri, int* v_pri) {
    g_h_lanes = h_lanes;
    g_v_lanes = v_lanes;

    g_h_pri = (int*)malloc(h_lanes * sizeof(int));
    g_v_pri = (int*)malloc(v_lanes * sizeof(int));
    for (int i = 0; i < h_lanes; i++) g_h_pri[i] = h_pri[i];
    for (int i = 0; i < v_lanes; i++) g_v_pri[i] = v_pri[i];

    g_drv_h = (int*)calloc(h_lanes, sizeof(int));
    g_drv_v = (int*)calloc(v_lanes, sizeof(int));

    g_ticket_h = (long long*)calloc(h_lanes, sizeof(long long));
    g_ticket_v = (long long*)calloc(v_lanes, sizeof(long long));
    g_served_h = (long long*)calloc(h_lanes, sizeof(long long));
    g_served_v = (long long*)calloc(v_lanes, sizeof(long long));

    g_front_seq_h = (long long*)malloc(h_lanes * sizeof(long long));
    g_front_seq_v = (long long*)malloc(v_lanes * sizeof(long long));
    for (int i = 0; i < h_lanes; i++) g_front_seq_h[i] = -1;
    for (int i = 0; i < v_lanes; i++) g_front_seq_v[i] = -1;

    g_arrival_seq = 0;

    pthread_mutex_init(&g_mutex, NULL);
    pthread_cond_init(&g_cond,  NULL);
}

void arrive_crossroad(int car_id, Direction dir, int lane) {
    pthread_mutex_lock(&g_mutex);

    hw2_write_output(car_id, ET_ARRIVE, dir, lane);

    long long my_ticket, my_seq;
    if (dir == DIR_HORIZONTAL) {
        my_ticket = g_ticket_h[lane]++;
        my_seq    = g_arrival_seq++;
        if (my_ticket == g_served_h[lane])
            g_front_seq_h[lane] = my_seq;
    } else {
        my_ticket = g_ticket_v[lane]++;
        my_seq    = g_arrival_seq++;
        if (my_ticket == g_served_v[lane])
            g_front_seq_v[lane] = my_seq;
    }

    while (1) {
        int at_front = (dir == DIR_HORIZONTAL)
            ? (my_ticket == g_served_h[lane])
            : (my_ticket == g_served_v[lane]);

        if (at_front && can_enter(dir, lane, my_seq))
            break;

        pthread_cond_wait(&g_cond, &g_mutex);

        if (dir == DIR_HORIZONTAL) {
            if (my_ticket == g_served_h[lane] && g_front_seq_h[lane] < 0)
                g_front_seq_h[lane] = my_seq;
        } else {
            if (my_ticket == g_served_v[lane] && g_front_seq_v[lane] < 0)
                g_front_seq_v[lane] = my_seq;
        }
    }

    if (dir == DIR_HORIZONTAL) {
        g_drv_h[lane]++;
        g_served_h[lane]++;
        g_front_seq_h[lane] = -1;
    } else {
        g_drv_v[lane]++;
        g_served_v[lane]++;
        g_front_seq_v[lane] = -1;
    }

    hw2_write_output(car_id, ET_ENTER, dir, lane);

    pthread_cond_broadcast(&g_cond);

    pthread_mutex_unlock(&g_mutex);
}

void exit_crossroad(int car_id, Direction dir, int lane) {
    pthread_mutex_lock(&g_mutex);

    if (dir == DIR_HORIZONTAL)
        g_drv_h[lane]--;
    else
        g_drv_v[lane]--;

    hw2_write_output(car_id, ET_EXIT, dir, lane);

    pthread_cond_broadcast(&g_cond);

    pthread_mutex_unlock(&g_mutex);
}