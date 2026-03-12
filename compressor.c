/*
 * Huffman File Compressor
 * Extension: .gidds
 *
 * File Format:
 * [MAGIC: 4 bytes "HUFF"]
 * [TABLE SIZE: 2 bytes - number of unique chars]
 * [FREQ TABLE: n * (1 byte char + 4 byte freq) entries]
 * [PADDING BITS: 1 byte - how many bits to ignore at end]
 * [COMPRESSED DATA]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAX_CHARS 256
#define MAGIC "HUFF"
#define EXT ".gidds"

/* Huffman Node */
typedef struct Node {
    unsigned char ch;
    uint32_t freq;
    struct Node *left, *right;
} Node;

/* Min-Heap (priority queue) */
typedef struct {
    Node **data;
    int size, cap;
} Heap;

/* Code table */
typedef struct {
    char bits[MAX_CHARS]; /* '0'/'1' string */
    int len;
} Code;

/* Heap helpers */
Heap *heap_new(int cap) {
    Heap *h = malloc(sizeof(Heap));
    h->data = malloc(cap * sizeof(Node *));
    h->size = 0;
    h->cap = cap;
    return h;
}

void heap_push(Heap *h, Node *n) {
    int i = h->size++;
    h->data[i] = n;
    /* bubble up */
    while (i > 0) {
        int p = (i - 1) / 2;
        if (h->data[p]->freq <= h->data[i]->freq) break;
        Node *tmp = h->data[p]; 
        h->data[p] = h->data[i]; 
        h->data[i] = tmp;
        i = p;
    }
}

Node *heap_pop(Heap *h) {
    Node *top = h->data[0];
    h->data[0] = h->data[--h->size];
    /* bubble down */
    int i = 0;
    while (1) {
        int l = 2*i+1, r = 2*i+2, sm = i;
        if (l < h->size && h->data[l]->freq < h->data[sm]->freq) sm = l;
        if (r < h->size && h->data[r]->freq < h->data[sm]->freq) sm = r;
        if (sm == i) break;
        Node *tmp = h->data[sm]; 
        h->data[sm] = h->data[i]; 
        h->data[i] = tmp;
        i = sm;
    }
    return top;
}

Node *new_node(unsigned char ch, uint32_t freq, Node *l, Node *r) {
    Node *n = malloc(sizeof(Node));
    n->ch = ch; 
    n->freq = freq;
    n->left = l;  
    n->right = r;
    return n;
}

/* Build Huffman Tree */
/*
 * Algorithm:
 *   1. Pop two minimum nodes → make parent (freq = sum)
 *   2. Push parent back
 *   3. Repeat until 1 node left (the root)
 */
Node *build_tree(uint32_t freq[MAX_CHARS]) {
    Heap *h = heap_new(MAX_CHARS * 2);

    for (int i = 0; i < MAX_CHARS; i++)
        if (freq[i]) heap_push(h, new_node((unsigned char)i, freq[i], NULL, NULL));

    /* Edge case: single unique character */
    if (h->size == 1) {
        Node *only = heap_pop(h);
        Node *root = new_node(0, only->freq, only, NULL);
        free(h->data); 
        free(h);
        return root;
    }

    while (h->size > 1) {
        Node *a = heap_pop(h); /* minimum */
        Node *b = heap_pop(h); /* second minimum */
        heap_push(h, new_node(0, a->freq + b->freq, a, b));
    }
    Node *root = heap_pop(h);
    free(h->data); 
    free(h);
    return root;
}

/* Generate Code Table */
void gen_codes(Node *n, char *buf, int depth, Code table[MAX_CHARS]) {
    if (!n->left && !n->right) { /* leaf */
        memcpy(table[n->ch].bits, buf, depth);
        table[n->ch].bits[depth] = '\0';
        table[n->ch].len = depth ? depth : 1; /* single-char file edge case */
        if (!depth) { 
            table[n->ch].bits[0] = '0'; 
            table[n->ch].bits[1] = '\0';
        }
        return;
    }
    if (n->left)  { 
        buf[depth] = '0'; 
        gen_codes(n->left,  buf, depth+1, table); 
    }
    if (n->right) { 
        buf[depth] = '1'; 
        gen_codes(n->right, buf, depth+1, table);
    }
}

/* Bit Writer */
typedef struct {
    FILE *f;
    unsigned char buf;
    int bit_count; /* bits filled in buf */
    long padding_pos; /* file offset of padding byte */
} BitWriter;

BitWriter *bw_open(FILE *f) {
    BitWriter *bw = calloc(1, sizeof(BitWriter));
    bw->f = f;
    bw->padding_pos = ftell(f); /* save position BEFORE writing padding placeholder */
    fputc(0, f);                /* placeholder for padding byte */
    return bw;
}

void bw_write_bit(BitWriter *bw, int bit) {
    bw->buf = (bw->buf << 1) | (bit & 1);
    if (++bw->bit_count == 8) {
        fputc(bw->buf, bw->f);
        bw->buf = 0; 
        bw->bit_count = 0;
    }
}

int bw_flush(BitWriter *bw) {
    int padding = 0;
    if (bw->bit_count > 0) {
        padding = 8 - bw->bit_count;
        bw->buf <<= padding; /* pad with 0s on right */
        fputc(bw->buf, bw->f);
    }
    /* Go back and write actual padding count */
    long end = ftell(bw->f);
    fseek(bw->f, bw->padding_pos, SEEK_SET);
    fputc((unsigned char)padding, bw->f);
    fseek(bw->f, end, SEEK_SET);
    free(bw);
    return padding;
}

/* COMPRESS */
void compress(const char *in_path) {
    FILE *in = fopen(in_path, "rb");
    if (!in) { 
        perror("open input"); 
        return; 
    }

    /* 1. Count frequencies */
    uint32_t freq[MAX_CHARS] = {0};
    int c;
    while ((c = fgetc(in)) != EOF) freq[c]++;
    rewind(in);

    /* 2. Build tree & code table */
    Node *root  = build_tree(freq);
    Code table[MAX_CHARS] = {0};
    char tmp[MAX_CHARS];
    gen_codes(root, tmp, 0, table);

    /* 3. Create output path: original_name.gidds */
    char out_path[512];
    snprintf(out_path, sizeof(out_path), "%s%s", in_path, EXT);
    FILE *out = fopen(out_path, "wb");
    if (!out) { 
        perror("open output"); 
        fclose(in); 
        return; 
    }

    /* 4. Write header */
    fwrite(MAGIC, 1, 4, out);

    /* Count unique chars for table */
    uint16_t unique = 0;
    for (int i = 0; i < MAX_CHARS; i++) if (freq[i]) unique++;
    fwrite(&unique, 2, 1, out);

    /* Write freq table: [char(1)][freq(4)] per entry */
    for (int i = 0; i < MAX_CHARS; i++) {
        if (freq[i]) {
            fputc((unsigned char)i, out);
            fwrite(&freq[i], 4, 1, out);
        }
    }

    /* 5. Write compressed bits */
    BitWriter *bw = bw_open(out);
    while ((c = fgetc(in)) != EOF) {
        Code *cd = &table[(unsigned char)c];
        for (int i = 0; i < cd->len; i++)
            bw_write_bit(bw, cd->bits[i] == '1');
    }
    bw_flush(bw);

    fclose(in); fclose(out);
    printf("Compressed → %s\n", out_path);
}

/* Bit Reader */
typedef struct {
    FILE *f;
    unsigned char buf;
    int bits_left; /* unread bits in buf */
    int padding;
    long total_bits;
    long bits_read;
} BitReader;

int br_read_bit(BitReader *br) {
    if (br->bits_read >= br->total_bits) return -1;
    if (br->bits_left == 0) {
        int c = fgetc(br->f);
        if (c == EOF) return -1;
        br->buf = (unsigned char)c;
        br->bits_left = 8;
    }
    br->bits_read++;
    return (br->buf >> --br->bits_left) & 1;
}

/* DECOMPRESS */
void decompress(const char *in_path) {
    FILE *in = fopen(in_path, "rb");
    if (!in) { 
        perror("open input"); 
        return; 
    }

    /* Read & verify magic */
    char magic[5] = {0};
    fread(magic, 1, 4, in);
    if (strcmp(magic, MAGIC) != 0) {
        printf("Not a valid .gidds file!\n"); 
        fclose(in); 
        return;
    }

    /* Read freq table */
    uint16_t unique;
    fread(&unique, 2, 1, in);

    uint32_t freq[MAX_CHARS] = {0};
    uint64_t total_chars = 0;
    for (int i = 0; i < unique; i++) {
        unsigned char ch = fgetc(in);
        uint32_t f;
        fread(&f, 4, 1, in);
        freq[ch] = f;
        total_chars += f;
    }

    /* Read padding byte */
    int padding = fgetc(in);

    /* Rebuild tree from freq table */
    Node *root = build_tree(freq);

    /* Calculate total bits to read */
    long data_start = ftell(in);
    fseek(in, 0, SEEK_END);
    long data_end = ftell(in);
    fseek(in, data_start, SEEK_SET);
    long total_bytes = data_end - data_start;
    long total_bits  = total_bytes * 8 - padding;

    /* Setup bit reader */
    BitReader br = {in, 0, 0, padding, total_bits, 0};

    /* Build output path: strip .gidds, add _decompressed */
    char out_path[512];
    strncpy(out_path, in_path, sizeof(out_path)-1);
    char *ext = strstr(out_path, EXT);
    if (ext) *ext = '\0'; /* strip .gidds */
    strncat(out_path, "_out", sizeof(out_path) - strlen(out_path) - 1);
    /* Keep original extension if any was before .gidds */

    FILE *out = fopen(out_path, "wb");
    if (!out) { 
        perror("open output"); 
        fclose(in); 
        return; 
    }

    /* Decode bits using tree */
    uint64_t written = 0;
    Node *cur = root;
    while (written < total_chars) {
        int bit = br_read_bit(&br);
        if (bit == -1) break;

        cur = bit ? cur->right : cur->left;
        if (!cur) break;

        if (!cur->left && !cur->right) { /* leaf = decoded char */
            fputc(cur->ch, out);
            written++;
            cur = root;
        }
    }

    fclose(in); fclose(out);
    printf("Decompressed → %s\n", out_path);
}

/* Main */
int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage:\n");
        printf("  %s -c <file>        → compress file → file.gidds\n", argv[0]);
        printf("  %s -d <file.gidds>   → decompress\n", argv[0]);
        return 1;
    }
    if (strcmp(argv[1], "-c") == 0) compress(argv[2]);
    else if (strcmp(argv[1], "-d") == 0) decompress(argv[2]);
    else { 
        printf("Unknown flag: %s\n", argv[1]); 
        return 1; 
    }
    return 0;
}