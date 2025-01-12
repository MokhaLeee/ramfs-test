#pragma once

int sls(const char *pathname);
int scat(const char *pathname);
int smkdir(const char *pathname);
int stouch(const char *pathname);
int secho(const char *content);
int swhich(const char *cmd);
void init_shell();
void close_shell();