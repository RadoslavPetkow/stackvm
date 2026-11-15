// Двупасов асемблер: превежда .asm текст в .bin байткод за VM-а.
//   Пас 1: намира етикетите и адресите им (пази ги в свързан списък).
//   Пас 2: генерира байтовете, като замества етикетите с адреси.
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vm.h"

#define LINE_MAX_LEN 256
#define LABEL_MAX    64

// ---------- Свързан списък с етикети ----------

typedef struct LabelNode {
    char name[LABEL_MAX];
    uint32_t address;
    struct LabelNode *next;
} LabelNode;

static LabelNode *label_find(LabelNode *head, const char *name) {
    for (LabelNode *n = head; n != NULL; n = n->next) {
        if (strcmp(n->name, name) == 0) return n;
    }
    return NULL;
}

// Добавя етикет в началото на списъка. Връща false при липса на памет.
static bool label_add(LabelNode **head, const char *name, uint32_t address) {
    LabelNode *n = malloc(sizeof(*n));
    if (n == NULL) return false;
    snprintf(n->name, sizeof(n->name), "%s", name);
    n->address = address;
    n->next = *head;
    *head = n;
    return true;
}

static void label_free_all(LabelNode *head) {
    while (head != NULL) {
        LabelNode *next = head->next;
        free(head);
        head = next;
    }
}

// ---------- Динамичен буфер за байткода ----------

typedef struct {
    uint8_t *data;
    size_t size;
    size_t capacity;
} ByteBuf;

static bool buf_push(ByteBuf *b, uint8_t byte) {
    if (b->size == b->capacity) {
        size_t new_cap = (b->capacity == 0) ? 64 : b->capacity * 2;
        uint8_t *p = realloc(b->data, new_cap);
        if (p == NULL) return false;
        b->data = p;
        b->capacity = new_cap;
    }
    b->data[b->size++] = byte;
    return true;
}

static bool buf_push_i32_le(ByteBuf *b, int32_t value) {
    uint32_t u = (uint32_t)value;
    return buf_push(b, (uint8_t)(u & 0xFF)) &&
           buf_push(b, (uint8_t)((u >> 8) & 0xFF)) &&
           buf_push(b, (uint8_t)((u >> 16) & 0xFF)) &&
           buf_push(b, (uint8_t)((u >> 24) & 0xFF));
}

// ---------- Обработка на ред ----------

// Маха коментара (от ';') и интервалите в двата края. Връща указател в същия буфер.
static char *clean_line(char *line) {
    char *comment = strchr(line, ';');
    if (comment != NULL) *comment = '\0';

    while (isspace((unsigned char)*line)) line++;

    size_t len = strlen(line);
    while (len > 0 && isspace((unsigned char)line[len - 1])) {
        line[--len] = '\0';
    }
    return line;
}

// Ако редът е етикет ("име:"), маха двоеточието и връща true.
static bool is_label_line(char *line) {
    size_t len = strlen(line);
    if (len < 2 || line[len - 1] != ':') return false;
    line[len - 1] = '\0';
    return true;
}

static bool valid_label_name(const char *name) {
    if (!(isalpha((unsigned char)name[0]) || name[0] == '_')) return false;
    for (const char *p = name; *p != '\0'; p++) {
        if (!(isalnum((unsigned char)*p) || *p == '_')) return false;
    }
    return strlen(name) < LABEL_MAX;
}

// Търси мнемониката в общата таблица. Връща NULL, ако я няма.
static const InstrInfo *find_instr(const char *mnemonic) {
    for (int i = 0; i < OP_COUNT; i++) {
        if (strcmp(OPCODE_TABLE[i].name, mnemonic) == 0) return &OPCODE_TABLE[i];
    }
    return NULL;
}

// Опитва да прочете цяло число, което се побира в int32_t.
static bool parse_int32(const char *text, int32_t *out) {
    char *end = NULL;
    errno = 0;
    long v = strtol(text, &end, 0);
    if (end == text || *end != '\0' || errno == ERANGE) return false;
    if (v < INT32_MIN || v > INT32_MAX) return false;
    *out = (int32_t)v;
    return true;
}

// Разделя "PUSH 5" на мнемоника и аргумент (аргументът може да е NULL).
// Връща false, ако на реда има повече от две думи.
static bool split_line(char *line, char **mnemonic, char **operand) {
    *mnemonic = strtok(line, " \t");
    *operand = strtok(NULL, " \t");
    return strtok(NULL, " \t") == NULL;
}

// ---------- Двата паса ----------

static bool pass1(FILE *in, LabelNode **labels) {
    char raw[LINE_MAX_LEN];
    uint32_t offset = 0;
    int line_no = 0;

    while (fgets(raw, sizeof(raw), in) != NULL) {
        line_no++;
        char *line = clean_line(raw);
        if (*line == '\0') continue;

        if (is_label_line(line)) {
            if (!valid_label_name(line)) {
                fprintf(stderr, "ред %d: невалидно име на етикет '%s'\n", line_no, line);
                return false;
            }
            if (label_find(*labels, line) != NULL) {
                fprintf(stderr, "ред %d: етикетът '%s' вече е дефиниран\n", line_no, line);
                return false;
            }
            if (!label_add(labels, line, offset)) {
                fprintf(stderr, "няма памет\n");
                return false;
            }
            continue;
        }

        char *mnemonic, *operand;
        split_line(line, &mnemonic, &operand);
        const InstrInfo *info = find_instr(mnemonic);
        if (info == NULL) {
            fprintf(stderr, "ред %d: непозната инструкция '%s'\n", line_no, mnemonic);
            return false;
        }
        offset += info->has_operand ? 5 : 1;
    }
    return true;
}

static bool pass2(FILE *in, LabelNode *labels, ByteBuf *out) {
    char raw[LINE_MAX_LEN];
    int line_no = 0;

    while (fgets(raw, sizeof(raw), in) != NULL) {
        line_no++;
        char *line = clean_line(raw);
        if (*line == '\0' || is_label_line(line)) continue;

        char *mnemonic, *operand;
        if (!split_line(line, &mnemonic, &operand)) {
            fprintf(stderr, "ред %d: твърде много аргументи\n", line_no);
            return false;
        }
        const InstrInfo *info = find_instr(mnemonic); // проверено в пас 1

        if (!info->has_operand) {
            if (operand != NULL) {
                fprintf(stderr, "ред %d: %s не приема аргумент\n", line_no, mnemonic);
                return false;
            }
            if (!buf_push(out, (uint8_t)info->opcode)) return false;
            continue;
        }

        if (operand == NULL) {
            fprintf(stderr, "ред %d: %s изисква аргумент\n", line_no, mnemonic);
            return false;
        }

        int32_t value;
        if (!parse_int32(operand, &value)) {
            LabelNode *label = label_find(labels, operand);
            if (label == NULL) {
                fprintf(stderr, "ред %d: '%s' не е число и не е дефиниран етикет\n",
                        line_no, operand);
                return false;
            }
            value = (int32_t)label->address;
        }

        if (!buf_push(out, (uint8_t)info->opcode) || !buf_push_i32_le(out, value)) {
            fprintf(stderr, "няма памет\n");
            return false;
        }
    }
    return true;
}

// Записва хедър ("VM01" + размер, little-endian) и след него байткода.
static bool write_bin(const char *path, const ByteBuf *code) {
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        perror(path);
        return false;
    }

    uint32_t n = (uint32_t)code->size;
    uint8_t size_le[4] = {
        (uint8_t)(n & 0xFF), (uint8_t)((n >> 8) & 0xFF),
        (uint8_t)((n >> 16) & 0xFF), (uint8_t)((n >> 24) & 0xFF)
    };

    bool ok = fwrite(VM_MAGIC_BYTES, 1, 4, f) == 4 &&
              fwrite(size_le, 1, 4, f) == 4 &&
              fwrite(code->data, 1, code->size, f) == code->size;
    if (fclose(f) != 0) ok = false;
    if (!ok) fprintf(stderr, "грешка при запис в %s\n", path);
    return ok;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "употреба: %s програма.asm изход.bin\n", argv[0]);
        return 1;
    }

    FILE *in = fopen(argv[1], "r");
    if (in == NULL) {
        perror(argv[1]);
        return 1;
    }

    LabelNode *labels = NULL;
    ByteBuf code = {0};
    bool ok = pass1(in, &labels);

    if (ok) {
        rewind(in);
        ok = pass2(in, labels, &code);
    }
    fclose(in);

    if (ok) ok = write_bin(argv[2], &code);
    if (ok) printf("%s: %zu байта байткод\n", argv[2], code.size);

    label_free_all(labels);
    free(code.data);
    return ok ? 0 : 1;
}
