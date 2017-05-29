#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "stderr.h"

enum
{
    MAX_NUMBER_OF_NODES = 1500000,
    NUM_OF_ALPHA = 26,
    MAX_LENGTH_OF_WORD = 100,
    MAX_OCCURENCE_OF_SAME_WRONG_WORD = 10000,
    MAX_NUMBER_OF_WRONG_WORDS = 600000,
};
typedef long long ll;

static void build_trie(char s[]);
static bool contains(char s[]);
static ll get_word(char s[], FILE *in);
static int wrong_word_cmp(const void *p1, const void *p2);
static int ll_cmp(const void *p1, const void *p2);
static void spell_check(const char *dictionary, const char *article, const char *misspelling);

static int trie[MAX_NUMBER_OF_NODES][NUM_OF_ALPHA + 1];
static int next = 0;
static int debug = 0;
static char *trigger = "";
static int trigger_debug = 0;

static void build_trie(char s[])
{
    int i, t = 0;
    if (debug)
        printf("Build trie for: [%s]\n", s);
    for (i = 0; s[i] != '\0'; ++i)
    {
        if (debug)
            printf("Processing %d '%c': ", i, s[i]);
        if (isupper(s[i]))
        {
            s[i] = tolower(s[i]);
        }
        int pos = s[i] - 'a';
        if (trie[t][pos] == 0)
        {
            assert(next < MAX_NUMBER_OF_NODES);
            trie[t][pos] = ++next;
        }
        if (debug)
            printf("t = %d, p = %d, trie[t][p] = %d\n", t, pos, trie[t][pos]);
        t = trie[t][pos];
    }
    trie[t][NUM_OF_ALPHA] = 1;
}

static void dump_trie_from(const char *sofar, int node)
{
    int len = strlen(sofar);
    char buffer[len+2];
    strcpy(buffer, sofar);
    buffer[len + 1] = '\0';
    printf("sofar = [%s] (node %d) (%s)\n", sofar, node, trie[node][NUM_OF_ALPHA] == 1 ? "word" : "prefix");
    for (int i = 0; i < NUM_OF_ALPHA; i++)
    {
        if (trie[node][i] != 0)
        {
            buffer[len] = i + 'a';
            dump_trie_from(buffer, trie[node][i]);
        }
    }
    printf("finish [%s] (node %d)\n", sofar, node);
}

/* Check if the trie contains the string s */
static bool contains(char s[])
{
    ll i, t = 0;
    if (trigger_debug)
        printf("Scan for: [%s]\n", s);
    for (i = 0; s[i] != '\0'; ++i)
    {
        if (trigger_debug)
            printf("Check '%c': ", s[i]);
        assert(isalpha((unsigned char)s[i]));
        int pos = tolower((unsigned char)s[i]) - 'a';
        if (pos < 0 || pos > NUM_OF_ALPHA)
            fprintf(stderr, "Assertion: [%s] %lld == %c (pos = %d)\n", s, i, s[i], pos);
        assert(pos >= 0 && pos <= NUM_OF_ALPHA);
        if (trigger_debug)
            printf("t = %lld, pos = %d, trie[t][pos] = %d\n", t, pos, trie[t][pos]);
        if (trie[t][pos] == 0)
        {
            if (trigger_debug)
                printf("return false\n");
            return false;
        }
        t = trie[t][pos];
    }
    if (trie[t][NUM_OF_ALPHA] == 0)
    {
        if (trigger_debug)
            printf("return false\n");
        return false;
    }
    if (trigger_debug)
        printf("return true\n");
    return true;
}

static ll current_pos = 0;

static inline int f_getc(FILE *fp)
{
    int c = fgetc(fp);
    if (c != EOF)
        current_pos++;
    return c;
}

static ll get_word(char s[], FILE *in)
{
    ll c, begin_of_word = 0, lim = MAX_LENGTH_OF_WORD;
    char *w = s;
    while (!isalpha(c = f_getc(in)) && c != EOF)
        ;
    assert(isalpha(c) || c == EOF);
    if (c == EOF)
    {
        *w = '\0';
        return c;
    }
    *w++ = tolower(c);
    assert(current_pos > 0);
    begin_of_word = current_pos - 1;
    for ( ; --lim > 0; ++w)
    {
        if (!isalpha(c = f_getc(in)))
            break;
        *w = tolower(c);
    }
    *w = '\0';
    if (debug)
        printf("%s: %lld %zu [%s]\n", __func__, begin_of_word, strlen(s), s);
    return begin_of_word;
}

typedef struct WrongWord WrongWord;
struct WrongWord
{
    char word[MAX_LENGTH_OF_WORD];
    ll pos;
};

static int wrong_word_cmp(const void *p1, const void *p2)
{
    return strcmp((*(const WrongWord **)p1)->word, (*(const WrongWord **)p2)->word);
}

static int ll_cmp(const void *p1, const void *p2)
{
    return *((const ll **)p1) - *((const ll **)p2);
}

static WrongWord *wrong_word_list[MAX_NUMBER_OF_WRONG_WORDS];

static void read_dictionary(const char *dictionary)
{
    FILE *dict = fopen(dictionary, "r");
    if (dict == NULL)
        err_syserr("file '%s' cannot be opened for reading\n", dictionary);
    char word[MAX_LENGTH_OF_WORD];
    while (fgets(word, sizeof word, dict))
    {
        word[strcspn(word, "\r\n")] = 0;
        build_trie(word);
    }
    fclose(dict);
    if (debug)
        dump_trie_from("", 0);
}

static void spell_check(const char *dictionary, const char *article, const char *misspelling)
{
    /* Builds the trie from dictionary .*/
    read_dictionary(dictionary);

    FILE *in = fopen(article, "r");
    if (!in)
        err_syserr("file '%s' cannot be opened for reading\n", article);
    char str[MAX_LENGTH_OF_WORD];
    ll begin_of_word = 0;
    ll wrong_word_count = 0;
    while ((begin_of_word = get_word(str, in)) != EOF)
    {
        if (strcasecmp(trigger, str) == 0)
            trigger_debug = 1;
        if (!contains(str))
        {
            //WrongWord *wwp = malloc(sizeof wrong_word_list[0]);
            WrongWord *wwp = malloc(sizeof(*wwp));
            static int done = 0;
            if (debug && !done)
                printf("Sizes: %zu %zu %zu\n", sizeof(wrong_word_list[0]), sizeof(*wwp), sizeof(WrongWord)), done++;
            if (!wwp)
                err_error("Out of memory error!\n");
            if (debug)
                printf("%s: %lld %zu [%s] %lld\n", __func__, begin_of_word, strlen(str), str, wrong_word_count+1);
            strcpy(wwp->word, str);
            wwp->pos = begin_of_word;
            wrong_word_list[wrong_word_count++] = wwp;
        }
        trigger_debug = 0;
    }
    fclose(in);
    qsort(wrong_word_list, wrong_word_count, sizeof wrong_word_list[0], wrong_word_cmp);

    /* Adds a sentinel node. */
    wrong_word_list[wrong_word_count] = malloc(sizeof wrong_word_list[0]);
    strcpy(wrong_word_list[wrong_word_count++]->word, "");

    /* Prints the result into misspelling.txt */
    FILE *out = fopen(misspelling, "w");
    if (!out)
        err_syserr("file '%s' cannot be opened for writing\n", misspelling);
    char last_word[MAX_LENGTH_OF_WORD] = "";
    ll i, j, pos[MAX_OCCURENCE_OF_SAME_WRONG_WORD], count = 0;
    for (i = 0; i < wrong_word_count; ++i)
    {
        if (strcmp(last_word, wrong_word_list[i]->word))    /* Meets a new word. */
        {
            if (*last_word)    /* Prints the last word if exists. */
            {
                fprintf(out, "%s ", last_word);
                qsort(pos, count, sizeof pos[0], ll_cmp);
                for (j = 0; j < count; ++j)
                {
                    fprintf(out, "%lld%c", pos[j], j == count - 1 ? '\n' : ' ');
                }
            }
            count = 0;
            strcpy(last_word, wrong_word_list[i]->word);
            pos[count++] = wrong_word_list[i]->pos;
            assert(count < MAX_OCCURENCE_OF_SAME_WRONG_WORD);
        }
        else      /* Same word. */
        {
            pos[count++] = wrong_word_list[i]->pos;
            assert(count < MAX_OCCURENCE_OF_SAME_WRONG_WORD);
        }
    }
    fclose(out);

    /* free wrong word list */
    for (int i = 0; i < wrong_word_count; i++)
    {
        free(wrong_word_list[i]);
    }
    wrong_word_count = 0;

}

static const char optstr[] = "Dd:ha:o:t:";
static const char usestr[] = "[-Dh][-d dictionary][-a article][-o output][-t trigger]";
static const char hlpstr[] =
    "  -d dictionary  Use named dictionary file (default dictionary.txt)\n"
    "  -D             Enable debug output\n"
    "  -a article     Use named article file (default article.txt)\n"
    "  -h             Print this help message and exit\n"
    "  -o output      Use named file for output (default misspelling.txt)\n"
    "  -t trigger     Turn on detailed debugging when trigger word is read\n"
    ;

int main(int argc, char **argv)
{
    const char *def_dictionary = "dictionary.txt";
    const char *def_article = "article.txt";
    const char *def_misspelling = "misspelling.txt";
    const char *dictionary = 0;
    const char *article = 0;
    const char *misspelling = 0;

    err_setarg0(argv[0]);

    int opt;
    while ((opt = getopt(argc, argv, optstr)) != -1)
    {
        switch (opt)
        {
        case 'D':
            trigger_debug = 1;
            debug = 1;
            break;
        case 'd':
            dictionary = optarg;
            break;
        case 'h':
            err_help(usestr, hlpstr);
            /*NOTREACHED*/
        case 'a':
            article = optarg;
            break;
        case 'o':
            misspelling = optarg;
            break;
        case 't':
            trigger = optarg;
            break;
        default:
            err_usage(usestr);
            /*NOTREACHED*/
        }
    }
    if (argc != optind)
        err_usage(usestr);

    if (dictionary == 0)
        dictionary = def_dictionary;
    if (article == 0)
        article = def_article;
    if (misspelling == 0)
        misspelling = def_misspelling;

    spell_check(dictionary, article, misspelling);
    return 0;
}
