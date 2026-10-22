#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <math.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#define MAX_NORMAIS      2048ULL
#define MAX_SUBNORMAIS   2048ULL
#define MAX_COMPLETO     2048ULL
#define MAX_EXPOENTE       30
#define TAM               256

static const char digitos[] =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

static void escreve_base(char *saida, size_t capacidade,
                         unsigned long long m, int b, int k)
{
    char inverso[80], normal[80];
    int n = 0;
    do {
        inverso[n++] = digitos[m % (unsigned)b];
        m /= (unsigned)b;
    } while (m != 0);
    for (int i = 0; i < n; ++i) normal[i] = inverso[n - 1 - i];
    normal[n] = '\0';

    size_t pos = 0;
#define INSERE(c) do { if (pos + 1 < capacidade) saida[pos++] = (c); } while (0)
    if (k >= 0) {
        for (int i = 0; i < n; ++i) INSERE(normal[i]);
        for (int i = 0; i < k; ++i) INSERE('0');
    } else if (-k < n) {
        int antes = n + k;
        for (int i = 0; i < antes; ++i) INSERE(normal[i]);
        INSERE('.');
        for (int i = antes; i < n; ++i) INSERE(normal[i]);
    } else {
        INSERE('0'); INSERE('.');
        for (int i = 0; i < -k - n; ++i) INSERE('0');
        for (int i = 0; i < n; ++i) INSERE(normal[i]);
    }
    saida[pos] = '\0';
#undef INSERE
}

static void escreve_decimal(char *saida, size_t tamanho, long double valor)
{
    if (valor == 0.0L) { snprintf(saida, tamanho, "0"); return; }
    int ordem = (int)floorl(log10l(fabsl(valor)));
    int casas = 17 - ordem;
    if (casas < 0) casas = 0;
    if (casas > 120) casas = 120;
    snprintf(saida, tamanho, "%.*Lf", casas, valor);
    char *ponto = strchr(saida, '.');
    if (ponto) {
        size_t n = strlen(saida);
        while (n > 0 && saida[n - 1] == '0') saida[--n] = '\0';
        if (n > 0 && saida[n - 1] == '.') saida[--n] = '\0';
    }
}

static void celula(char *saida, size_t tamanho, int bloco,
                   unsigned long long mantissa, int b, int k)
{
    if (bloco == 0)
        snprintf(saida, tamanho, "%llu*%d^(%d)", mantissa, b, k);
    else if (bloco == 1)
        escreve_decimal(saida, tamanho,
            (long double)mantissa * powl((long double)b, k));
    else
        escreve_base(saida, tamanho, mantissa, b, k);
}

static void diferenca(char *saida, size_t tamanho, int bloco, int b, int k)
{
    if (bloco == 0) snprintf(saida, tamanho, "%d^(%d)", b, k);
    else if (bloco == 1) escreve_decimal(saida, tamanho, powl((long double)b, k));
    else escreve_base(saida, tamanho, 1, b, k);
}

static const char *titulo_bloco(int bloco)
{
    static const char *titulos[] = {
        "REPRESENTACAO SIMBOLICA",
        "VALORES DECIMAIS (APROXIMADOS)",
        "REPRESENTACAO NA BASE b"
    };
    return titulos[bloco];
}

static void imprime_normais(int bloco, int b, int p, int emin, int emax,
                            unsigned long long inicio, unsigned long long fim)
{
    size_t largura = 1;
    char texto[TAM];
    for (int e = emin; e <= emax; ++e)
        for (unsigned long long m = inicio; m <= fim; ++m) {
            celula(texto, sizeof(texto), bloco, m, b, e - p + 1);
            size_t n = strlen(texto);
            if (n > largura) largura = n;
        }
    printf("\nNORMAIS - %s\n", titulo_bloco(bloco));
    for (int e = emin; e <= emax; ++e) {
        int k = e - p + 1;
        for (unsigned long long m = inicio; m <= fim; ++m) {
            celula(texto, sizeof(texto), bloco, m, b, k);
            printf("%-*s  ", (int)largura, texto);
        }
        diferenca(texto, sizeof(texto), bloco, b, k);
        printf("| diferenca = %s\n", texto);
    }
}

static void imprime_subnormais(int bloco, int b, int p, int emin,
                               unsigned long long inicio)
{
    int k = emin - p + 1;
    unsigned long long fim = inicio - 1;
    size_t largura = 1;
    char texto[TAM];
    for (unsigned long long m = 1; m <= fim; ++m) {
        celula(texto, sizeof(texto), bloco, m, b, k);
        size_t n = strlen(texto);
        if (n > largura) largura = n;
    }
    printf("\nSUBNORMAIS - %s\n", titulo_bloco(bloco));
    for (unsigned long long m = 1; m <= fim; ++m) {
        celula(texto, sizeof(texto), bloco, m, b, k);
        printf("%-*s  ", (int)largura, texto);
    }
    diferenca(texto, sizeof(texto), bloco, b, k);
    printf("| diferenca = %s\n", texto);
}

static int escreve_dat_normais(const char *caminho, int b, int p,
                               int emin, int emax,
                               unsigned long long inicio,
                               unsigned long long fim)
{
    FILE *fp = fopen(caminho, "w");
    if (!fp) return 0;
    for (int e = emin; e <= emax; ++e) {
        long double unidade = powl((long double)b, e - p + 1);
        for (unsigned long long m = inicio; m <= fim; ++m)
            fprintf(fp, "%.19Lg %.19Lg\n",
                    (long double)m * unidade,
                    (long double)m * unidade);
    }
    fclose(fp);
    return 1;
}

static int escreve_dat_subnormais(const char *caminho, int b, int p,
                                  int emin, unsigned long long inicio)
{
    FILE *fp = fopen(caminho, "w");
    if (!fp) return 0;
    long double unidade = powl((long double)b, emin - p + 1);
    for (unsigned long long m = 1; m < inicio; ++m)
        fprintf(fp, "%.19Lg %.19Lg\n",
                (long double)m * unidade,
                (long double)m * unidade);
    fclose(fp);
    return 1;
}

static int escreve_meta(const char *caminho, int b, int p, int emin, int emax,
                        unsigned long long qn, unsigned long long qs,
                        long double maior_normal, long double maior_subnormal)
{
    FILE *fp = fopen(caminho, "w");
    if (!fp) return 0;
    fprintf(fp,
        "b=%d\np=%d\nemin=%d\nemax=%d\n"
        "qtd_normais=%llu\nqtd_subnormais=%llu\n"
        "maior_normal=%.19Lg\nmaior_subnormal=%.19Lg\n",
        b, p, emin, emax, qn, qs, maior_normal, maior_subnormal);
    fclose(fp);
    return 1;
}

static int le_inteiro(const char *s, int *destino)
{
    if (s == NULL || *s == '\0') return 0;
    char *fim = NULL;
    errno = 0;
    long v = strtol(s, &fim, 10);
    if (errno != 0 || fim == s || *fim != '\0') return 0;
    if (v < INT_MIN || v > INT_MAX) return 0;
    *destino = (int)v;
    return 1;
}

static void uso(const char *prog)
{
    fprintf(stderr,
        "Uso: %s <base> <precisao> <expoente_minimo> <modo> [dir]\n"
        "  base              : 2 a 36\n"
        "  precisao          : >= 1\n"
        "  expoente_minimo   : -%d ate 0\n"
        "  modo              : tabela | dados | gnuplot\n"
        "  dir               : diretorio de saida (modo dados)\n",
        prog, MAX_EXPOENTE);
}

int main(int argc, char **argv)
{
    if (argc < 5) { uso(argv[0]); return EXIT_FAILURE; }

    int b, p, emin;
    if (!le_inteiro(argv[1], &b) || b < 2 || b > 36) {
        fprintf(stderr, "Base invalida: '%s'.\n", argv[1]); return EXIT_FAILURE;
    }
    if (!le_inteiro(argv[2], &p) || p < 1) {
        fprintf(stderr, "Precisao invalida: '%s'.\n", argv[2]); return EXIT_FAILURE;
    }
    if (!le_inteiro(argv[3], &emin) || emin > 0 || emin < -MAX_EXPOENTE) {
        fprintf(stderr, "Expoente minimo invalido: '%s'.\n", argv[3]); return EXIT_FAILURE;
    }

    const char *modo = argv[4];
    int modo_tabela = strcmp(modo, "tabela") == 0;
    int modo_dados  = strcmp(modo, "dados")  == 0;
    if (!modo_tabela && !modo_dados) {
        fprintf(stderr, "Modo desconhecido: '%s'.\n", modo); return EXIT_FAILURE;
    }
    if (modo_dados && argc < 6) {
        fprintf(stderr, "Modo 'dados' requer diretorio.\n"); return EXIT_FAILURE;
    }

    int emax = 1 - emin;
    unsigned long long potencia = 1;
    for (int i = 0; i < p; ++i) {
        if (potencia > ULLONG_MAX / (unsigned)b) {
            fprintf(stderr, "b^p excede o limite inteiro.\n");
            return EXIT_FAILURE;
        }
        potencia *= (unsigned)b;
    }

    unsigned long long inicio = potencia / (unsigned)b;
    unsigned long long fim = potencia - 1;
    unsigned long long por_linha = fim - inicio + 1;
    unsigned long long linhas = (unsigned long long)(emax - emin + 1);

    if (por_linha > MAX_NORMAIS / linhas) {
        fprintf(stderr, "Ha mais de %llu normais; limite excedido.\n",
                MAX_NORMAIS);
        return EXIT_FAILURE;
    }

    unsigned long long qtd_normais = por_linha * linhas;
    unsigned long long qtd_subnormais = inicio - 1;

    int completos = 0;
    if (qtd_subnormais > 0 &&
        qtd_subnormais <= MAX_SUBNORMAIS &&
        qtd_normais <= MAX_COMPLETO &&
        qtd_subnormais <= MAX_COMPLETO - qtd_normais)
        completos = 1;

    long double maior_normal =
        (long double)fim * powl((long double)b, emax - p + 1);
    long double maior_subnormal =
        (long double)(inicio - 1) * powl((long double)b, emin - p + 1);

    if (modo_dados) {
        const char *dir = argv[5];
        mkdir(dir, 0755);
        char cn[512], cs[512], cm[512];
        snprintf(cn, sizeof cn, "%s/normais.dat", dir);
        snprintf(cs, sizeof cs, "%s/subnormais.dat", dir);
        snprintf(cm, sizeof cm, "%s/meta.txt", dir);
        if (!escreve_dat_normais(cn, b, p, emin, emax, inicio, fim) ||
            !escreve_dat_subnormais(cs, b, p, emin, inicio) ||
            !escreve_meta(cm, b, p, emin, emax,
                          qtd_normais, qtd_subnormais,
                          maior_normal, maior_subnormal)) {
            fprintf(stderr, "Falha ao escrever em '%s'.\n", dir);
            return EXIT_FAILURE;
        }
        printf("normais=%llu\nsubnormais=%llu\ncompletos=%d\n",
               qtd_normais, qtd_subnormais, completos);
        return EXIT_SUCCESS;
    }

    /* modo tabela */
    printf("F(%d, %d, %d, %d), com emax = 1 - emin.\n", b, p, emin, emax);
    printf("Normais positivos: %llu. Subnormais positivos: %llu.\n",
           qtd_normais, qtd_subnormais);

    for (int bloco = 0; bloco < 3; ++bloco)
        imprime_normais(bloco, b, p, emin, emax, inicio, fim);

    if (qtd_subnormais == 0) {
        printf("\nEste sistema nao possui subnormais positivos (p = 1).\n");
        return EXIT_SUCCESS;
    }
    if (!completos) {
        printf("\nSubnormais excedem o limite de tabela; "
               "exibindo apenas os normais.\n");
        return EXIT_SUCCESS;
    }

    for (int bloco = 0; bloco < 3; ++bloco)
        imprime_subnormais(bloco, b, p, emin, inicio);

    return EXIT_SUCCESS;
}
