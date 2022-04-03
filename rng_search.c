/*
 * rng_search
 * Copyright © 2022, Cambridge State Machines
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define WD_ALPHA 256
#define MAX_WD 32768
#define MAX_LEN 32

typedef struct {
    unsigned char ch;
    unsigned int prv;
    unsigned int nxt;
    unsigned int sta;
} PTN;

typedef struct {
    unsigned int id;
    unsigned int len;
} IDR;

PTN *get_wd_ptn(unsigned char **, int);
int *get_wd_lens(unsigned char **, int, int);
void proc_wd(unsigned char *, int, PTN *, int *, IDR *);
void get_wd_hit(unsigned char *txt, IDR *, int, int, int, int, int);
int save_ptn(const char *, PTN *, int);
int save_lens(const char *, int *, int);
PTN *get_ptn_from_file(const char *);
int *get_lens_from_file(const char *);

unsigned char *get_txt(const char *, int *);
unsigned char **init_wd_mtrx(int, int);
int **init_int_mtrx(int, int);
unsigned char **get_wd_mtrx(unsigned char *, int, int *);
IDR *init_hits(int);
void free_wd_mtrx(unsigned char **, int);
void free_int_mtrx(int **, int);
void report(IDR *, int);
void report_wd(IDR *hits, unsigned char *, int);
int proc_1(void);
int proc_2(void);

/* MAIN FUNCTIONS */

PTN *get_wd_ptn(unsigned char **wd_mtrx, int wd_count)
{
    int i, j, ptn_count = WD_ALPHA, **int_mtrx = NULL;
    PTN *ptn = NULL;
    if (!(int_mtrx = init_int_mtrx(wd_count, MAX_LEN))) return NULL;
    if (!(ptn = malloc((WD_ALPHA + (wd_count * MAX_LEN)) * sizeof(*ptn)))) return NULL;
    memset(ptn, 0, (WD_ALPHA + (wd_count * MAX_LEN)) * sizeof(*ptn));
    for (j = 0; j < MAX_LEN; j++) {
        for (i = 1; i < wd_count; i++) {
            if (j == 0) {
                if (!ptn[wd_mtrx[i][0]].sta) {
                    ptn[wd_mtrx[i][0]].ch = wd_mtrx[i][0];
                    ptn[wd_mtrx[i][0]].sta = i;
                }
                int_mtrx[i][0] = wd_mtrx[i][0];
            } else {
                if (wd_mtrx[i][j]) {
                    if (strncasecmp((const char *)wd_mtrx[i - 1], (const char *)wd_mtrx[i], j + 1)) {
                        int_mtrx[i][j] = ptn_count;
                        ptn[ptn_count].ch = wd_mtrx[i][j];
                        ptn[ptn_count].prv = int_mtrx[i][j - 1];
                        if (!ptn[int_mtrx[i][j - 1]].nxt) ptn[int_mtrx[i][j - 1]].nxt = ptn_count;
                        ptn[ptn_count].sta = i;
                        ptn_count++;
                    } else {
                        int_mtrx[i][j] = int_mtrx[i - 1][j];
                    }
                }
            }
        }
    }
    //save_ptn("ptn.dat", ptn, ptn_count);
    free_int_mtrx(int_mtrx, wd_count);
    if (!(ptn = realloc(ptn, ptn_count * sizeof(*ptn)))) return NULL;
    return ptn;
}

int *get_wd_lens(unsigned char **wd_mtrx, int rows, int cols)
{
    int i, *wd_lens = NULL;
    if (!(wd_lens = malloc(rows * sizeof(*wd_lens)))) return NULL;
    memset(wd_lens, 0, rows * sizeof(*wd_lens));
    for (i = 0; i < rows; i++) {
        wd_lens[i] = strnlen((const char *)wd_mtrx[i], cols);
    }
    //save_lens("lens.dat", wd_lens, rows);
    return wd_lens;
}

void proc_wd(unsigned char *txt, int fsz, PTN *ptn, int *lens, IDR *hits)
{
    int txt_pos = 0, curr_sta = 0, wd_pos, curr_ptn, prev_ptn;
    while (txt_pos < fsz) {
        while (!ptn[tolower(txt[txt_pos])].sta) txt_pos++;
        wd_pos = 1;
        while (1) {
            if (wd_pos == 1) {
                curr_sta = txt_pos;
                prev_ptn = tolower(txt[txt_pos]);
                curr_ptn = ptn[tolower(txt[txt_pos])].nxt;
            } else {
                while (ptn[curr_ptn].prv == prev_ptn && ptn[curr_ptn].ch != tolower(txt[txt_pos])) curr_ptn++;
                if (ptn[curr_ptn].prv != prev_ptn) break;
                prev_ptn = curr_ptn;
                curr_ptn = ptn[curr_ptn].nxt;
            }
            get_wd_hit(txt, hits, txt_pos++, curr_sta, wd_pos++, ptn[prev_ptn].sta, lens[ptn[prev_ptn].sta]);
        }
        while (isalpha(txt[txt_pos])) txt_pos++;
    }
}

void get_wd_hit(unsigned char *txt, IDR *hits, int txt_pos, int curr_sta, int wd_pos, int wd_id, int wd_len)
{
    if (!isalpha(txt[txt_pos + 1]) && wd_pos == wd_len) {
        hits[curr_sta].id = wd_id;
        hits[curr_sta].len = wd_len;
    }
}

int save_ptn(const char *fnm, PTN *wd_ptn, int ptn_count)
{
    FILE *output_file = NULL;
    if (!(output_file = fopen(fnm, "wb"))) return 1;
    if (fwrite(wd_ptn, sizeof(*wd_ptn), ptn_count, output_file) != ptn_count) return 1;
    fclose(output_file);
    return 0;
}

int save_lens(const char *fnm, int *lens, int wd_count)
{
    FILE *output_file = NULL;
    if (!(output_file = fopen(fnm, "wb"))) return 1;
    if (fwrite(lens, sizeof(*lens), wd_count, output_file) != wd_count) return 1;
    fclose(output_file);
    return 0;
}

PTN *get_ptn_from_file(const char *fnm)
{
    int ptn_count;
    PTN *ptn = NULL;
    FILE *input_file = NULL;
    if (!(input_file = fopen(fnm, "rb"))) return NULL;
    fseek(input_file, 0, SEEK_END);
    ptn_count = ftell(input_file) / sizeof(*ptn);
    rewind(input_file);
    if (!(ptn = malloc(ptn_count * sizeof(*ptn)))) return NULL;
    memset(ptn, 0, ptn_count * sizeof(*ptn));
    if (fread(ptn, sizeof(*ptn), ptn_count, input_file) != ptn_count) return NULL;
    fclose(input_file);
    return ptn;
}

int *get_lens_from_file(const char *fnm)
{
    int *lens = NULL, wd_count;
    FILE *input_file = NULL;
    if (!(input_file = fopen(fnm, "rb"))) return NULL;
    fseek(input_file, 0, SEEK_END);
    wd_count = ftell(input_file) / sizeof(*lens);
    rewind(input_file);
    if (!(lens = malloc(wd_count * sizeof(*lens)))) return NULL;
    memset(lens, 0, wd_count * sizeof(*lens));
    if (fread(lens, sizeof(*lens), wd_count, input_file) != wd_count) return NULL;
    fclose(input_file);
    return lens;
}

/* UTILITY FUNCTIONS */

unsigned char *get_txt(const char *fnm, int *fsz)
{
    unsigned char *txt = NULL;
    FILE *input_file = NULL;
    if (!(input_file = fopen(fnm, "rb"))) return NULL;
    fseek(input_file, 0, SEEK_END);
    *fsz = ftell(input_file) + 1;
    rewind(input_file);
    if (!(txt = malloc(*fsz))) return NULL;
    memset(txt, 0, *fsz);
    if (fread(txt, 1, *fsz - 1, input_file) != *fsz - 1) return NULL;
    fclose(input_file);
    return txt;
}

unsigned char **init_wd_mtrx(int rows, int cols)
{
    unsigned char **wd_mtrx = NULL;
    int i;
    if (!(wd_mtrx = malloc(rows * sizeof(*wd_mtrx)))) return NULL;
    memset(wd_mtrx, 0, rows * sizeof(*wd_mtrx));
    for (i = 0; i < rows; i++) {
        if (!(wd_mtrx[i] = malloc(cols * sizeof(**wd_mtrx)))) return NULL;
        memset(wd_mtrx[i], 0, cols * sizeof(**wd_mtrx));
    }
    return wd_mtrx;
}

int **init_int_mtrx(int rows, int cols)
{
    int i, **int_mtrx = NULL;
    if (!(int_mtrx = malloc(rows * sizeof(*int_mtrx)))) return NULL;
    memset(int_mtrx, 0, rows * sizeof(*int_mtrx));
    for (i = 0; i < rows; i++) {
        if (!(int_mtrx[i] = malloc(cols * sizeof(**int_mtrx)))) return NULL;
        memset(int_mtrx[i], 0, cols * sizeof(**int_mtrx));
    }
    return int_mtrx;
}

unsigned char **get_wd_mtrx(unsigned char *txt, int fsz, int *wd_count)
{
    unsigned char **wd_mtrx = NULL;
    int i, col = 0;
    if (!(wd_mtrx = init_wd_mtrx(MAX_WD, MAX_LEN))) return NULL;
    for (i = 0; i < fsz; i++) {
        switch (txt[i]) {
            case 10:
                if ((*wd_count) < MAX_WD) (*wd_count)++;
                col = 0;
                break;
            case 13:
                break;
            default:
                if (col < MAX_LEN) wd_mtrx[(*wd_count)][col++] = txt[i];
                break;
        }
    }
    return wd_mtrx;
}

IDR *init_hits(int fsz)
{
    IDR *hits = NULL;
    if (!(hits = malloc(fsz * sizeof(*hits)))) return NULL;
    memset(hits, 0, fsz * sizeof(*hits));
    return hits;
}

void free_wd_mtrx(unsigned char **wd_mtrx, int rows)
{
    int i;
    for (i = 0; i < rows; i++) {
        free(wd_mtrx[i]);
    }
    free(wd_mtrx);
}

void free_int_mtrx(int **int_mtrx, int rows)
{
    int i;
    for (i = 0; i < rows; i++) {
        free(int_mtrx[i]);
    }
    free(int_mtrx);
}

void report(IDR *hits, int fsz)
{
    int i, wd_count = 0;
    for (i = 0; i < fsz; i++) {
        if (hits[i].id) wd_count++;
    }
    printf("%d hits\n", wd_count);
}

void report_wd(IDR *hits, unsigned char *txt, int fsz)
{
    int i, j;
    printf("txt_pos,wd_id,len,word\n");
    for (i = 0; i < fsz; i++) {
        if (hits[i].id) {
            printf("%d,%d,%d,", i, hits[i].id, hits[i].len);
            for (j = i; j < i + hits[i].len; j++) {
                printf("%c", txt[j]);
            }
            printf("\n");
        }
    }
}

int proc_1()
{
    unsigned char *txt = NULL, *wd_txt = NULL, **wd_mtrx = NULL;
    int fsz, ms, wd_fsz, wd_count = 1, *lens = NULL;
    IDR *hits = NULL;
    PTN *ptn = NULL;
    clock_t start_time = clock();
    if (!(wd_txt = get_txt("wds.txt", &wd_fsz))) return 1;
    if (!(wd_mtrx = get_wd_mtrx(wd_txt, wd_fsz, &wd_count))) return 1;
    if (!(lens = get_wd_lens(wd_mtrx, wd_count, MAX_LEN))) return 1;
    if (!(ptn = get_wd_ptn(wd_mtrx, wd_count))) return 1;
    if (!(txt = get_txt("read_file.txt", &fsz))) return 1;
    if (!(hits = init_hits(fsz))) return 1;
    proc_wd(txt, fsz, ptn, lens, hits);
    ms = (clock() - start_time) * 1000 / CLOCKS_PER_SEC;
    printf("%d chars\n", fsz - 1);
    report(hits, fsz); //show hit count
    //report_wd(hits, txt, fsz); //show all hits
    printf("%dms\n", ms);
    free(wd_txt);
    free_wd_mtrx(wd_mtrx, wd_count);
    free(lens);
    free(ptn);
    free(txt);
    free(hits);
    getchar();
    return 0;
}

int proc_2()
{
    unsigned char *txt = NULL;
    int fsz, ms, *lens = NULL;
    IDR *hits = NULL;
    PTN *ptn = NULL;
    clock_t start_time = clock();
    if (!(lens = get_lens_from_file("lens.dat"))) return 1;
    if (!(ptn = get_ptn_from_file("ptn.dat"))) return 1;
    if (!(txt = get_txt("read_file.txt", &fsz))) return 1;
    if (!(hits = init_hits(fsz))) return 1;
    proc_wd(txt, fsz, ptn, lens, hits);
    ms = (clock() - start_time) * 1000 / CLOCKS_PER_SEC;
    printf("%d chars\n", fsz - 1);
    report(hits, fsz); //show hit count
    //report_wd(hits, txt, fsz); //show all hits
    printf("%dms\n", ms);
    free(lens);
    free(ptn);
    free(txt);
    free(hits);
    getchar();
    return 0;
}

int main()
{
    if (proc_1()) return 1; //generate ptn from dictionary (with optional saving of ptn.dat and lens.dat)
    //if (proc_2()) return 1; //load pre-saved ptn and lens from files (requires ptn.dat and lens.dat)
    getchar();
    return 0;
}
