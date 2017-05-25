/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

#include "pingpong.h"
#include <stdlib.h>
#include <stdio.h>
#include <ucontext.h>

#define STACKSIZE 32768 /*Tamanho da pilha de threads*/

task_t tarefa_principal, *tarefa_atual;
int count = 0;

// funções gerais ==============================================================

// Inicializa o sistema operacional; deve ser chamada no inicio do main()

void pingpong_init() {

    setvbuf(stdout, 0, _IONBF, 0); //desativa o buffer de saida padrao (stdout), usado pela função printf.
    tarefa_principal.id = count++; //ID da tarefa
    tarefa_principal.parent = NULL; //A primeira tarefa nunca possuirá pai
    tarefa_principal.status = EXECUTANDO; //..assim que ela é criada, já entra em execução
    tarefa_atual = &tarefa_principal; //..como ela é a tarefa em execução no momento, o vetor tarefa_atual começa apontando pra ela

}

// gerência de tarefas =========================================================

// Cria uma nova tarefa. Retorna um ID> 0 ou erro.

int task_create (task_t *task,			// descritor da nova tarefa
                 void (*start_func)(void *),	// funcao corpo da tarefa
                 void *arg) {			// argumentos para a tarefa   
    
    //Exceptions
    if (!task) {
        perror("Tarefa vazia: ");
        return -1;
    }

    task->status = NOVA;
    getcontext(&(task->context));

    char *pilha = (char *) malloc (STACKSIZE);

    if (pilha) {
        task->context.uc_stack.ss_sp = pilha;
        task->context.uc_stack.ss_size = STACKSIZE;
        task->context.uc_stack.ss_flags = 0;
        task->context.uc_link = 0;
        task->id = count++;
        task->parent = tarefa_atual;

    } else {
        perror("Erro ao criar pilha");
        return -1;
    }

    makecontext (&task->context, (void*)(*start_func), 1, arg); //Associa o contexto à função passada por argumento    
    task->status = PRONTA; //Finalizada as inicializações da tarefa

#ifdef DEBUG
    
    printf("task_create: criou a tarefa  nro %d\n", task->id);
    
#endif 

    return task->id;
}

// Termina a tarefa corrente, indicando um valor de status encerramento

void task_exit(int exitCode) {

    if(tarefa_atual->id == 0){
        exit(exitCode);
    }
    
    task_t *tarefa_final = tarefa_atual;
    tarefa_atual = &tarefa_principal;
    tarefa_final->status = TERMINADA;
    tarefa_atual->status = EXECUTANDO;
    
    #ifdef DEBUG
    printf("task_exit: tarefa %d sendo encerrado com codigo %d\n", tarefa_final->id, exitCode);
    #endif // DEBUG

    //Efetua a troca de contexto da a última tarefa e a tarefa principal
    swapcontext(&tarefa_final->context, &tarefa_atual->context);
}

// alterna a execução para a tarefa indicada

int task_switch(task_t *task) {
    //Exceptions
    if(!task){
        perror("Tarefa vazia: ");
        return -1;
    }
    
    task_t *tarefa_final = tarefa_atual;
    tarefa_atual = task;
    tarefa_final->status = PRONTA;
    tarefa_atual->status = EXECUTANDO;
    
    #ifdef DEBUG
    printf("task_switch: trocando contexto %d -> %d\n", tarefa_final->id, tarefa_atual->id);
    #endif // DEBUG

    //Troca o contexto entre as tarefas passadas como parâmetro
    swapcontext(&tarefa_final->context, &tarefa_atual->context);
    return 0;

}

// retorna o identificador da tarefa corrente (main eh 0)

int task_id() {
    return tarefa_atual->id;
}

