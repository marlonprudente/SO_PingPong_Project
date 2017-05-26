// PingPongOS - PingPong Operating System
// Prof. Carlos A. Maziero, DAINF UTFPR
// Versão 1.0 -- Março de 2015
//
// Estruturas de dados internas do sistema operacional

#ifndef __DATATYPES__
#define __DATATYPES__

#include <ucontext.h>
#include "queue.h"

//Estado de uma tarefa (conforme diagrama de estados): Nova, Pronta, Suspensa e Terminada.
enum status_t{NOVO, PRONTO, EXECUTANDO, SUSPENSO, FINALIZADO};

//Estrutura com a informação do dono da tarefa, se é do usuário ou do sistema, para implementar o quantum
typedef enum task_dono{ USUARIO, SISTEMA
}task_dono_t;

typedef unsigned int sys_clock_t;
typedef unsigned long long count_t;

// Estrutura que define uma tarefa
typedef struct task_t
{
    struct task_t *prev;    //Próxima tarefa da fila
    struct task_t *next;    //Tarefa anterior da fila
    int id;                //Id da tarefa
    ucontext_t context;     //Contexto da tarefa
    enum status_t status;   //Estado da tarefa
    struct task_t *parent;  //"Pai" da tarefa (tarefa em execução quando esta tarefa foi criada)
    struct queue_t **fila_atual;
    int prio_estat;
    int prio_dinam;

    task_dono_t task_dono; //De quem é a tarefa, do Usuário ou do sistema, para controle do quantum

    sys_clock_t t_inicio;
    sys_clock_t t_executado;
    count_t contador_processo;

} task_t ;

// estrutura que define um semáforo
typedef struct
{
  // preencher quando necessário
} semaphore_t ;

// estrutura que define um mutex
typedef struct
{
  // preencher quando necessário
} mutex_t ;

// estrutura que define uma barreira
typedef struct
{
  // preencher quando necessário
} barrier_t ;

// estrutura que define uma fila de mensagens
typedef struct
{
  // preencher quando necessário
} mqueue_t ;

#endif
