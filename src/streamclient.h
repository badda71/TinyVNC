/*
 * TinyVNC - A VNC client for Nintendo 3DS
 *
 * streamclient.h - functions for handling mp3 audio streaming
 *
 * Copyright 2020 Sebastian Weber
 */

int start_stream(char *url, char *username, char *password);
void stop_stream();
int run_stream();
