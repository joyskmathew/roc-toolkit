/*
 * Copyright (c) 2018 Roc authors
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* Roc sender example.
 *
 * This example generates a 5-second sine wave and sends it to the receiver.
 * Receiver address and ports and other parameters are hardcoded.
 *
 * Building:
 *   gcc sender_sinewave.c -lroc
 *
 * Running:
 *   ./a.out
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <roc/context.h>
#include <roc/endpoint.h>
#include <roc/log.h>
#include <roc/sender.h>

/* Receiver parameters. */
#define EXAMPLE_RECEIVER_IP "127.0.0.1"
#define EXAMPLE_RECEIVER_SOURCE_PORT 10001
#define EXAMPLE_RECEIVER_REPAIR_PORT 10002

/* Signal parameters */
#define EXAMPLE_SAMPLE_RATE 44100
#define EXAMPLE_SINE_RATE 440
#define EXAMPLE_SINE_SAMPLES (EXAMPLE_SAMPLE_RATE * 5)
#define EXAMPLE_BUFFER_SIZE 100

#define oops(msg)                                                                        \
    do {                                                                                 \
        fprintf(stderr, "oops: %s\n", msg);                                              \
        exit(1);                                                                         \
    } while (0)

static void gensine(float* samples, size_t batch_num, size_t num_samples) {
    double t = batch_num * num_samples / 2;
    size_t i;
    for (i = 0; i < num_samples / 2; i++) {
        const float s =
            (float)sin(2 * 3.14159265359 * EXAMPLE_SINE_RATE / EXAMPLE_SAMPLE_RATE * t)
            * 0.1f;

        /* Fill samples for left and right channels. */
        samples[i * 2] = s;
        samples[i * 2 + 1] = -s;

        t += 1;
    }
}

int main() {
    /* Enable debug logging. */
    roc_log_set_level(ROC_LOG_DEBUG);

    /* Initialize context config.
     * Initialize to zero to use default values for all fields. */
    roc_context_config context_config;
    memset(&context_config, 0, sizeof(context_config));

    /* Create context.
     * Context contains memory pools and the network worker thread(s).
     * We need a context to create a sender. */
    roc_context* context;
    if (roc_context_open(&context_config, &context) != 0) {
        oops("roc_context_open");
    }

    /* Initialize sender config.
     * Initialize to zero to use default values for unset fields. */
    roc_sender_config sender_config;
    memset(&sender_config, 0, sizeof(sender_config));

    /* Setup input frame format. */
    sender_config.frame_sample_rate = EXAMPLE_SAMPLE_RATE;
    sender_config.frame_channels = ROC_CHANNEL_SET_STEREO;
    sender_config.frame_encoding = ROC_FRAME_ENCODING_PCM_FLOAT;

    /* Turn on internal CPU timer.
     * Sender must send packets with steady rate, so we should either implement
     * clocking or ask the library to do so. We choose the second here. */
    sender_config.clock_source = ROC_CLOCK_INTERNAL;

    /* Create sender. */
    roc_sender* sender;
    if (roc_sender_open(context, &sender_config, &sender) != 0) {
        oops("roc_sender_open");
    }

    /* Connect sender to the receiver source (audio) packets endpoint.
     * The receiver should expect packets with RTP header and Reed-Solomon (m=8) FECFRAME
     * Source Payload ID on that port. */
    roc_endpoint* recv_source_endp = NULL;
    if (roc_endpoint_allocate(&recv_source_endp) != 0) {
        oops("roc_endpoint_allocate");
    }

    roc_endpoint_set_protocol(recv_source_endp, ROC_PROTO_RTP_RS8M_SOURCE);
    roc_endpoint_set_host(recv_source_endp, EXAMPLE_RECEIVER_IP);
    roc_endpoint_set_port(recv_source_endp, EXAMPLE_RECEIVER_SOURCE_PORT);

    if (roc_sender_connect(sender, ROC_INTERFACE_AUDIO_SOURCE, recv_source_endp) != 0) {
        oops("roc_sender_connect");
    }

    if (roc_endpoint_deallocate(recv_source_endp) != 0) {
        oops("roc_endpoint_deallocate");
    }

    /* Connect sender to the receiver repair (FEC) packets endpoint.
     * The receiver should expect packets with Reed-Solomon (m=8) FECFRAME
     * Repair Payload ID on that port. */
    roc_endpoint* recv_repair_endp = NULL;
    if (roc_endpoint_allocate(&recv_repair_endp) != 0) {
        oops("roc_endpoint_allocate");
    }

    roc_endpoint_set_protocol(recv_repair_endp, ROC_PROTO_RS8M_REPAIR);
    roc_endpoint_set_host(recv_repair_endp, EXAMPLE_RECEIVER_IP);
    roc_endpoint_set_port(recv_repair_endp, EXAMPLE_RECEIVER_REPAIR_PORT);

    if (roc_sender_connect(sender, ROC_INTERFACE_AUDIO_REPAIR, recv_repair_endp) != 0) {
        oops("roc_sender_connect");
    }

    if (roc_endpoint_deallocate(recv_repair_endp) != 0) {
        oops("roc_endpoint_deallocate");
    }

    /* Generate sine wave and write it to the sender. */
    size_t i;
    for (i = 0; i < EXAMPLE_SINE_SAMPLES / EXAMPLE_BUFFER_SIZE; i++) {
        /* Generate sine wave. */
        float samples[EXAMPLE_BUFFER_SIZE];
        gensine(samples, i, EXAMPLE_BUFFER_SIZE);

        /* Write samples to the sender. */
        roc_frame frame;
        memset(&frame, 0, sizeof(frame));

        frame.samples = samples;
        frame.samples_size = EXAMPLE_BUFFER_SIZE * sizeof(float);

        if (roc_sender_write(sender, &frame) != 0) {
            oops("roc_sender_write");
        }
    }

    /* Destroy sender. */
    if (roc_sender_close(sender) != 0) {
        oops("roc_sender_close");
    }

    /* Destroy context. */
    if (roc_context_close(context) != 0) {
        oops("roc_context_close");
    }

    return 0;
}