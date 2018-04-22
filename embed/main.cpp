#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <gsl/gsl_rng.h>
#include "linelib.h"
#include "ransampl.h"

#define MAX_PATH_LENGTH 100

char entity_file[MAX_STRING], relation_file[MAX_STRING], net_file[MAX_STRING], triple_file[MAX_STRING], output_en_file[MAX_STRING], output_rl_file[MAX_STRING], output_ct_file[MAX_STRING], input_en_file[MAX_STRING], input_rl_file[MAX_STRING], input_ct_file[MAX_STRING];
int binary = 0, num_threads = 1, vector_size = 100, negative = 5, init = 0;
long long samples = 1, edge_count_actual;
real alpha = 0.025, ratio = 0.5;

const gsl_rng_type * gsl_T;
gsl_rng * gsl_r;

line_node node_w, node_c, node_r;
line_hin hin_wc;
line_trainer_line trainer_wc;
line_triple trip_wc;

double func_rand_num()
{
    return gsl_rng_uniform(gsl_r);
}

void *training_thread(void *id)
{
    long long edge_count = 0, last_edge_count = 0;
    unsigned long long next_random = (long long)id;
    real *error_vec = (real *)calloc(vector_size, sizeof(real));
    
    while (1)
    {
        //judge for exit
        if (edge_count > samples / num_threads + 2) break;
        
        if (edge_count - last_edge_count > 1000)
        {
            edge_count_actual += edge_count - last_edge_count;
            last_edge_count = edge_count;
            printf("%cAlpha: %f Progress: %.3lf%%", 13, alpha, (real)edge_count_actual / (real)(samples + 1) * 100);
            fflush(stdout);
        }
        
        if ((double)(rand()) / (double)(RAND_MAX) < ratio) trainer_wc.train_sample_od3(alpha, negative, error_vec, func_rand_num, next_random);
        else trip_wc.train_sample(alpha, 1, 2, func_rand_num);
        
        edge_count += 1;
    }
    free(error_vec);
    pthread_exit(NULL);
}

void TrainModel() {
    long a;
    pthread_t *pt = (pthread_t *)malloc(num_threads * sizeof(pthread_t));
    
    gsl_rng_env_setup();
    gsl_T = gsl_rng_rand48;
    gsl_r = gsl_rng_alloc(gsl_T);
    gsl_rng_set(gsl_r, 314159265);
    
    node_w.init(entity_file, vector_size);
    node_c.init(entity_file, vector_size);
    node_r.init(relation_file, vector_size);
    
    hin_wc.init(net_file, &node_w, &node_c, 0);
    
    trainer_wc.init(&hin_wc, 0);
    
    trip_wc.init(triple_file, &node_w, &node_w, &node_r);
    
    if (init)
    {
        FILE *fi;
        int size, dim;
        char word[MAX_STRING], ch;
        real f;
        
        fi = fopen(input_en_file, "rb");
        fscanf(fi, "%d %d", &size, &dim);
        for (int k = 0; k != size; k++)
        {
            fscanf(fi, "%s", word);
            ch = fgetc(fi);
            for (int c = 0; c != dim; c++)
            {
                fread(&f, sizeof(real), 1, fi);
                node_w.get_vec()[k * dim + c] = f;
            }
        }
        fclose(fi);
        
        fi = fopen(input_ct_file, "rb");
        fscanf(fi, "%d %d", &size, &dim);
        for (int k = 0; k != size; k++)
        {
            fscanf(fi, "%s", word);
            ch = fgetc(fi);
            for (int c = 0; c != dim; c++)
            {
                fread(&f, sizeof(real), 1, fi);
                node_c.get_vec()[k * dim + c] = f;
            }
        }
        fclose(fi);
        
        fi = fopen(input_rl_file, "rb");
        fscanf(fi, "%d %d", &size, &dim);
        for (int k = 0; k != size; k++)
        {
            fscanf(fi, "%s", word);
            ch = fgetc(fi);
            for (int c = 0; c != dim; c++)
            {
                fread(&f, sizeof(real), 1, fi);
                node_r.get_vec()[k * dim + c] = f;
            }
        }
        fclose(fi);
    }
    
    clock_t start = clock();
    printf("Training:");
    for (a = 0; a < num_threads; a++) pthread_create(&pt[a], NULL, training_thread, (void *)a);
    for (a = 0; a < num_threads; a++) pthread_join(pt[a], NULL);
    printf("\n");
    clock_t finish = clock();
    printf("Total time: %lf\n", (double)(finish - start) / CLOCKS_PER_SEC);
    
    node_w.output(output_en_file, binary);
    node_r.output(output_rl_file, binary);
    node_c.output(output_ct_file, binary);
}

int ArgPos(char *str, int argc, char **argv) {
    int a;
    for (a = 1; a < argc; a++) if (!strcmp(str, argv[a])) {
        if (a == argc - 1) {
            printf("Argument missing for %s\n", str);
            exit(1);
        }
        return a;
    }
    return -1;
}

int main(int argc, char **argv) {
    int i;
    if (argc == 1) {
        printf("DPE\n\n");
        printf("Options:\n");
        printf("Parameters for training:\n");
        printf("\t-entity <file>\n");
        printf("\t\tA dictionary of all entities and words.\n");
        printf("\t-relation <file>\n");
        printf("\t\tThe set of all relations.\n");
        printf("\t-network <int>\n");
        printf("\t\tThe co-occurren network between entities and words.\n");
        printf("\t-triple <int>\n");
        printf("\t\tThe set of seed relation instances.\n");
        printf("\t-binary <int>\n");
        printf("\t\tSave the resulting vectors in binary moded; default is 0 (off)\n");
        printf("\t-size <int>\n");
        printf("\t\tSet size of word vectors; default is 100\n");
        printf("\t-negative <int>\n");
        printf("\t\tNumber of negative examples; default is 5, common values are 5 - 10 (0 = not used)\n");
        printf("\t-samples <int>\n");
        printf("\t\tSet the number of training samples as <int>Million\n");
        printf("\t-threads <int>\n");
        printf("\t\tUse <int> threads (default 1)\n");
        printf("\t-init <int>\n");
        printf("\t\tWhether initializing the embedding using pre-trained ones.\n");
        printf("\t-ratio <float>\n");
        printf("\t\tThe ratio of text and seed for training.\n");
        printf("\t-alpha <float>\n");
        printf("\t\tSet the starting learning rate; default is 0.025\n");
        return 0;
    }
    if ((i = ArgPos((char *)"-entity", argc, argv)) > 0) strcpy(entity_file, argv[i + 1]);
    if ((i = ArgPos((char *)"-relation", argc, argv)) > 0) strcpy(relation_file, argv[i + 1]);
    if ((i = ArgPos((char *)"-network", argc, argv)) > 0) strcpy(net_file, argv[i + 1]);
    if ((i = ArgPos((char *)"-triple", argc, argv)) > 0) strcpy(triple_file, argv[i + 1]);
    if ((i = ArgPos((char *)"-output-en", argc, argv)) > 0) strcpy(output_en_file, argv[i + 1]);
    if ((i = ArgPos((char *)"-output-ct", argc, argv)) > 0) strcpy(output_ct_file, argv[i + 1]);
    if ((i = ArgPos((char *)"-output-rl", argc, argv)) > 0) strcpy(output_rl_file, argv[i + 1]);
    if ((i = ArgPos((char *)"-input-en", argc, argv)) > 0) strcpy(input_en_file, argv[i + 1]);
    if ((i = ArgPos((char *)"-input-ct", argc, argv)) > 0) strcpy(input_ct_file, argv[i + 1]);
    if ((i = ArgPos((char *)"-input-rl", argc, argv)) > 0) strcpy(input_rl_file, argv[i + 1]);
    if ((i = ArgPos((char *)"-binary", argc, argv)) > 0) binary = atoi(argv[i + 1]);
    if ((i = ArgPos((char *)"-size", argc, argv)) > 0) vector_size = atoi(argv[i + 1]);
    if ((i = ArgPos((char *)"-negative", argc, argv)) > 0) negative = atoi(argv[i + 1]);
    if ((i = ArgPos((char *)"-samples", argc, argv)) > 0) samples = (long long)(atof(argv[i + 1])*1000000);
    if ((i = ArgPos((char *)"-alpha", argc, argv)) > 0) alpha = atof(argv[i + 1]);
    if ((i = ArgPos((char *)"-ratio", argc, argv)) > 0) ratio = atof(argv[i + 1]);
    if ((i = ArgPos((char *)"-threads", argc, argv)) > 0) num_threads = atoi(argv[i + 1]);
    if ((i = ArgPos((char *)"-init", argc, argv)) > 0) init = atoi(argv[i + 1]);
    TrainModel();
    return 0;
}