#include "pingpong.h"
#include "queue.h"
#include <stdlib.h>
#include <stdio.h>
#include <ucontext.h>
//p005=======================================================
#include <signal.h>
#include <sys/time.h>
//===========================================================
//#define DEBUG_ALL             //    > ativa todos debugs
//#define DEBUG
//DEBUG_OPERATIONAL_SYSTEM  > ativa mensagens do estado do sistema operacional
//#define DEBUG_TASK_CREATE        // > ativa debugs para criacao de tarefa
#define DEBUG_TASK_EXIT         //  > ativa debugs para finalizacao de tarefa
#define DEBUG_TASK_EXIT_STATUS   // > ativa mensagem de estado da tarefa em sua finalizacao
//#define DEBUG_TASK_SWITCH         //> ativa debug para troca de tarefas
//DEBUG_TASK_SUSPEND        > ativas debug para suspensao de tarefas
//#define DEBUG_DISPATCHER        //  > habilita debug do dispatcher
//DEBUG_TASK_PRIORITIES     > habilita debug para prioridades de tarefas
//#define DEBUG_SYSTEM_TASK      //   > habilita mensagens para tarefas de sistema
//DEBUG_MINIMAL             > mostra principais mensagens de debug

#define STACKSIZE 32768		/* tamanho de pilha das threads */
#define ALPHA -1            /* taxa de envelhecimento de tarefas */
#define ERROR 32          /* buffer de string para mensagem de erro */
#define STANDARD_PRIO 0          /* valor padrão de prioridade ao criar uma tarefa */
#define PRIO_MAX -20        /* valor da prioridade máxima para tarefas */
#define PRIO_MIN 20         /* valor da prioridade mínima para tarefas */
//p05=======================================================
#define TICK_SEG       0           /* segundos que compoem um tick (somando com TICK_MICROS)*/
#define TICK_MSEG     1000        /* microssegundos que compoem um tick (somando com TICK_SECS)- 1 milissegundo neste caso*/
#define QUANTUM         20          /* ticks que compõem um quantum*/

///Variáveis globais    ========================================================
task_t tarefa_principal, dispatcher, *tarefa_atual = NULL, *fila_tprontas = NULL;     //Tarefa em execução

#define between(A,B,C) ((A-C>0)?(A>B&&B>B):((A==C)?(A==B&&B==C):(A<B&&B<C)))

int userTasks = 0;      //Contador de tarefas de usuário ativas
int id_count = 0;       //Contador de IDs
//p05======================================================================
int quantum_count = 0; //Contador de ticks para chegar a um quantum

// estrutura que define um tratador de sinal (deve ser global ou static)
struct sigaction action ;

// estrutura de inicialização to timer
struct itimerval timer;

//p06=====================================================================
sys_clock_t sys_clock_ms = 0;   //tempo do sistema em ms


///Funções P03 ============================================================
//inicializa o temporizador do sistema //p06
void init_timer_system();

//Inicializa variáveis da tarefa principal
void init_tarefa_principal();

//Funções p07=============================================================
void init_dispatcher();

//execuçao de uma interrupcao pelo temporizador (a cada tick)
void timer_tick();

//Função despachante de tarefas (corpo associada à tarefa despachante)
void dispatcher_body(void *arg);

//Despachante de tarefas
task_t *scheduler();

//Altera o estado de uma tarefa para PRONTA e adicona na fila de tarefas prontas
int task_set_ready(task_t* task);

//Altera o estado de uma tarefa para EXECUTANDO e à retira da fila da qual pertence
int task_set_executing(task_t* task);

///Funções P04 ============================================================
//Envelhece uma lista de tarefas
void task_get_old(task_t* task_excluded);

//Altera a prioridade dinâmica de uma tarefa
void task_set_dinamic_prio(task_t* task, int prio);

//Retorna a prioridade dinâmica de uma tarefa
int task_get_dinamic_prio(task_t *task);

//Incrementa em alpha a prioridade dinâmica de uma tarefa
void task_alpha_dinamic_prio(task_t* task, int alpha);

//Compara as prioridade de duas tarefas
int task_compare(task_t *task1, task_t *task2);


///Funções P05 ============================================================


//Ordena uma lista de tarefas
task_t* prioridade_max(task_t **task_q, int task_comp(task_t*,task_t*));



// funções gerais ==============================================================
// Inicializa o sistema operacional; deve ser chamada no inicio do main()
void pingpong_init (){
    //desativa o buffer de saida padrao (stdout), usado pela função printf
    setvbuf(stdout, 0, _IONBF, 0);

    init_timer_system();    

    init_tarefa_principal();           //Inicializa tarefa principal (atual)
    init_dispatcher();


    userTasks = 1;
   id_count = 1;


    #if defined(DEBUG_ALL) || defined(DEBUG_OPERATIONAL_SYSTEM)
    printf("pingpong_init: sistema inicialzou em %u\n", systime());
    #endif // defined(DEBUG_ALL)
}

// gerência de tarefas =========================================================
// Cria uma nova tarefa. Retorna um ID> 0 ou erro.
int task_create (task_t *task, void (*start_func)(void *), void *arg){
    //Checagem de erros
    if(!task){
        perror ("Tarefa não alocada corretamente: ");
        return -1;
    }

    task->status = NOVO;                 //Tarefa criada, mas não inicializada

    //A tarefa criada não pertence a nenhuma fila (por enquanto)
    task->next = NULL;
    task->prev = NULL;
    task->fila_atual = NULL;

    getcontext (&(task->context));      //Incializa com contexto atual
    char *stack = malloc (STACKSIZE);   //Inicialização da pilha

    //Inicialização do contexto da tarefa
    if (stack){
        task->context.uc_stack.ss_sp = stack;
        task->context.uc_stack.ss_size = STACKSIZE;
        task->context.uc_stack.ss_flags = 0;
        task->context.uc_link = 0;
       // task->prio_estat = STANDARD_PRIO;
        //task->prio_dinam = STANDARD_PRIO;
        task->id = id_count++;         //Novo ID
        task->parent = tarefa_atual;    //Tarefa corrente é a criadora desta tarefa
        task->task_dono = USUARIO;
        //p06
        task->t_executado = 0;
        task->t_inicio = systime();
        task->contador_processo = 0;

        task_setprio(task, STANDARD_PRIO);    //Prioridade default
    	task_set_dinamic_prio(task, task_getprio(task));

    }
    else{
        char error[32];
        sprintf(error, "Erro na criação da pilha da tarefa %d em %ums", task->id, task->t_inicio);
        perror (error);
        exit(-1);
    }

    makecontext (&task->context, (void*)(*start_func), 1, arg);     //Associa o contexto à função passada por argumento

    //Caso seja uma tarefa de usuário (ID > 1)
    if(task->task_dono == USUARIO){

        userTasks++;                //Nova tarefa de usuário criada

        if(task_set_ready(task)){    //Tenta mudar seu estado para PRONTO e inserir na fila de prontos
        
            char error[32];
            sprintf(error, "Erro ao mudar estado da tarefa %d para PRONTA.", task->id);
            perror (error);
            exit(-1);
        }
    }
    else{
        task->status = PRONTO;       //Apenas muda o estado, caso seja tarefa principal ou despachante
    }

    #if defined(DEBUG_ALL) || defined(DEBUG_TASK_CREATE) || defined(DEBUG_MINIMAL)
    printf("task_create: criou a tarefa %d em %ums\n", task->id, task->t_inicio);
    printf("Valor do Quantum %d\n", quantum_count);
    #endif // defined(DEBUG_ALL)

    return task->id;
}

// Termina a tarefa corrente, indicando um valor de status encerramento
void task_exit (int exitCode){
    //Se a tarefa corrente for a principal, finaliza com código da saída
   // if(tarefa_atual->id == 0){
  //      exit (exitCode);
  //  }

    task_t *last_task = tarefa_atual;   //Última tarefa em execução

    if(last_task->id == -1){             //Caso o despachante tente sair...
        tarefa_atual = &tarefa_principal;      //... a próxima tarefa será a principal, ...
    }
    else{
        tarefa_atual = &dispatcher;     //... caso uma tarefa de usuário tente sair, o despachante será o próximo a executar
        userTasks--;
    }
    
    last_task->status = FINALIZADO;       //Tarefa atual será finalizada

    if(tarefa_atual->task_dono == USUARIO){

        task_set_executing(tarefa_atual);

    }else{
        tarefa_atual->status = EXECUTANDO;   //Próxima tarefa entrará em execução
    }
    

    #ifdef DEBUG
    printf("task_exit: tarefa %d sendo encerrado com codigo %d\n", last_task->id, exitCode);
    #endif // DEBUG

    #if defined(DEBUG_ALL) || defined(DEBUG_TASK_EXIT) || defined(DEBUG_TASK_EXIT_STATUS) || defined(DEBUG_MINIMAL)
    #if !defined(DEBUG_SYSTEM_TASK)
    if(last_task->task_dono == USUARIO)
    #endif // defined(DEBUG_ALL)
        printf("Task %d exited: running time %u ms, CPU time %u ms, %lld activations\n",
            last_task->id, systime()-last_task->t_inicio, last_task->t_executado, last_task->contador_processo);
    #endif

    //Efetua a troca de contexto da a última tarefa e a tarefa principal
    swapcontext(&last_task->context, &tarefa_atual->context);
}

// alterna a execução para a tarefa indicada
int task_switch (task_t *task){
    //Checagem de erros
    if(!task){
        perror ("Tarefa não alocada corretamente: ");
        return -1;
    }

    task_t *last_task = tarefa_atual;   //Última tarefa executada
    tarefa_atual = task;                //Troca da tarefa antiga para a atual

    if(last_task->task_dono == USUARIO) //Caso seje uma tarefa de usuário...
    {
        task_set_ready(last_task); //Insere a tarefa corrente na fila de prontas, mudando seu estado para PRONTO, ...
        task_set_executing(tarefa_atual);
    }
    else{
        last_task->status = PRONTO;   //Caso contrário, apenas muda seu estado para PRONTO
        tarefa_atual->status = EXECUTANDO;
    }

   // if(tarefa_atual->id > 1) //Caso seje uma tarefa de usuário...
 //   {
 //       task_set_executing(task); //Insere a tarefa corrente na fila de prontas, mudando seu estado para PRONTO, ...
  //  }
 //   else{
 //       tarefa_atual->status = EXECUTANDO;   //Caso contrário, apenas muda seu estado para PRONTO
  //  }

    quantum_count = QUANTUM;

    #if defined(DEBUG_ALL) || defined(DEBUG_TASK_SWITCH) || defined(DEBUG_MINIMAL)
    printf("task_switch: trocando contexto %d -> %d (tarefa criada em %ums executada %lld vezes)\n",
        last_task->id, tarefa_atual->id, tarefa_atual->t_inicio, tarefa_atual->contador_processo);
    #endif // defined(DEBUG_ALL)

    tarefa_atual->contador_processo++;

    //Troca o contexto entre as tarefas passadas como parâmetro
    swapcontext(&last_task->context, &tarefa_atual->context);

    return 0;
}

// retorna o identificador da tarefa corrente (main eh 0)
int task_id (){
    return tarefa_atual->id;
}

// suspende uma tarefa, retirando-a de sua fila atual, adicionando-a à fila
// queue e mudando seu estado para "suspensa"; usa a tarefa atual se task==NULL
void task_suspend (task_t *task, task_t **queue){
    task_t * working_task;

    if(task){                            //Caso passado uma tarefa como parâmetro...
        working_task = task;            //... se trabalhará com ela, ...
    }
    else{
        working_task = tarefa_atual;    //... caso contrário, utilize a tarefa em execução.
    }

    working_task->status = SUSPENSO;   //Suspende a tarefa em trabalho

    if(queue){ //Se for passado uma fila como parâmetro...
    
        if(working_task->fila_atual){ //... verifique se a tarefa está conida em alguma fila, ...
            queue_remove(working_task->fila_atual, (queue_t *) working_task); //... se estiver, remova-a da fila atual e, ...
        }
        queue_append((queue_t **) queue,(queue_t *) working_task);  //... em seguida, adicione à fila passado por parâmetro, ...
        working_task->fila_atual = (queue_t **) queue;    //... atualizando para a nova fila em que se encontra.
    }

    #if defined(DEBUG_ALL) || defined(DEBUG_TASK_SUSPEND) || defined(DEBUG_MINIMAL)
        printf("task_suspend: tarefa %d entrando em suspensão.\n", working_task->id);
    #endif  //defined(DEBUG_ALL)


    //Volta para o despachante, caso a tarefa seja a corrente
    if(!task){
        task_switch(&dispatcher);
    }
        
}

// acorda uma tarefa, retirando-a de sua fila atual, adicionando-a à fila de
// tarefas prontas ("ready queue") e mudando seu estado para "pronta"
void task_resume (task_t *task){
    
    task_set_ready(task);

    #if defined(DEBUG_ALL) || defined(DEBUG_TASK_SUSPEND) || defined(DEBUG_MINIMAL)
    printf("task_resume: tarefa %d preparada para execução\n", task->id);
    #endif  //defined(DEBUG_ALL)
}

// operações de escalonamento ==================================================

// libera o processador para a próxima tarefa, retornando à fila de tarefas
// prontas ("ready queue")
void task_yield (){
    
    #if defined(DEBUG_ALL) || defined(DEBUG_TASK_SWITCH) || defined(DEBUG_MINIMAL)
    printf("task_yield: liberando-se da tarefa %d\n", tarefa_atual->id);
    #endif  //defined(DEBUG_ALL)

    /*Tarefa de usuário é sempre maior que 1
    if(tarefa_atual->id > 1) { //Caso seje uma tarefa de usuário...
   
        if(task_set_ready(tarefa_atual)){ //Insere a tarefa corrente na fila de prontas, mudando seu estado para PRONTO, ...        
            char error[32];
            sprintf(error, "Erro ao mudar estado da tarefa %d para PRONTA.", tarefa_atual->id);
            perror (error);
            exit(-1);
        }
    }
    else{
             tarefa_atual->status = PRONTO;   //Caso contrário, apenas muda seu estado para PRONTO
    }  
*/
    //Retorna para o despachante
    task_switch(&dispatcher);
}

//Mostra o ID de uma tarefa na tela (para debug)
#if defined(DEBUG_ALL) || defined(DEBUG_DISPATCHER) || defined(DEBUG_OPERATIONAL_SISTEM)
    void task_print(void* task_v){
        task_t *task = (task_t *) task_v;
        printf("<%d>", task->id);
    }
#endif //defined(DEBUG_ALL)

//Corpo de função da tarefa despachante
void dispatcher_body(void *arg){
    
    dispatcher.status = EXECUTANDO;  //Despachante em execução
    
    while(userTasks) {           //Enquanto houver tarefas de usuários
    
        task_t* next = scheduler(); //Próxima tarefa dada pelo escalonador

        if(next){
            
              #if defined(DEBUG_ALL) || defined(DEBUG_DISPATCHER) || defined(DEBUG_OPERATIONAL_SISTEM) || defined(DEBUG_MINIMAL)
                printf("dispatcher_body: tarefa %d a ser executada\n", next->id);
            #endif // defined(DEBUG_ALL)
            #if defined(DEBUG_ALL) || defined(DEBUG_DISPATCHER) || defined(DEBUG_OPERATIONAL_SISTEM)
                queue_print("Tarefas",(queue_t *)fila_tprontas, task_print);
            #endif //defined(DEBUG_ALL)

            if(task_set_executing(next)){    //Muda estado da próxima tarefa para EXECUTANDO e retira-a da fila atual
            
                char error[32];
                sprintf(error, "Erro ao mudar estado da tarefa %d para EXECUTANDO.", next->id);
                perror(error);
                exit(-1);
            }
            //dispatcher.status = PRONTO;      //Preparando despachante para troca de tarefa
            task_switch(next);              //Executa a próxima tarefa
           // dispatcher.status = EXECUTANDO;  //Ao voltar da última tarefa, despachante entra em execução
        }
        else if (!fila_tprontas){
            break;
        }
    }
    
    task_exit(0);
}


//Função do escalonador
task_t *scheduler(){
    
    //Se a fila de tarefas prontas estiver vazia, retorne nulo
    if(!fila_tprontas){
        return NULL;
    }

    
    
    //FCFS- ṕrimeiro elemento da fila será o próximo a executar
    task_t *next = prioridade_max(&fila_tprontas, task_compare);

    task_get_old(next);
	task_set_dinamic_prio(next, task_getprio(next));
    //Prepara a próxima tarefa para a próxima execução
    //fila_tprontas = fila_tprontas->next;


    #if defined(DEBUG_ALL) || defined(DEBUG_TASK_PRIORITIES) || defined(DEBUG_DISPATCHER) || defined(DEBUG_OPERATIONAL_SISTEM)
        printf("scheduler: tarefa %d selecionada\n", next->id);
    #endif //defined(DEBUG_ALL)
    
    return next;
}

//Inicializa tarefa principal
void init_tarefa_principal(){
    
    //A tarefa principal não pertence a nenhuma fila
    tarefa_principal.next = NULL;
    tarefa_principal.prev = NULL;
    tarefa_principal.fila_atual = NULL;

    tarefa_principal.id = id_count++;     //ID da tarefa principal
    tarefa_principal.parent = NULL;        //A primeira tarefa não possui pai,...
    tarefa_principal.status = EXECUTANDO;   //... já está em execução quando foi criada ...
    tarefa_principal.task_dono = USUARIO; //Tarefa do sistema
    //p06====
    tarefa_principal.t_inicio = 0;
    tarefa_principal.t_executado = 0;
    tarefa_principal.contador_processo = 1;

    task_setprio(&tarefa_principal, STANDARD_PRIO);    //Prioridade default
    task_set_dinamic_prio(&tarefa_principal, task_getprio(&tarefa_principal));

    userTasks++;

    if(task_set_ready(&tarefa_principal)){  //Tenta mudar seu estado para PRONTO e inserir na fila de prontos
    
        char error[32];
        sprintf(error, "Erro ao mudar estado da tarefa principal para PRONTA.");
        perror (error);
        exit(-1);
    }

    tarefa_atual = &tarefa_principal;      //... e é a tarefa em execução no momento.

    #ifdef DEBUG
    printf("init_main_task: tarefa principal iniciada com id %d", tarefa_principal.id);
    #endif  //DEBUG

}
void init_dispatcher(){

    task_create(&dispatcher, dispatcher_body, "dispatcher :");   //Inicializa despachante de tarefas
    dispatcher.task_dono = SISTEMA;
    dispatcher.id = -1;
}

//Função interna para ajudar a mudar o estado de uma tarefa e inseri-la na fila de prontas
//Retorna 0 caso ocorra tudo certo, -1 caso haja um erro
int task_set_ready(task_t* task){

        task->status = PRONTO;       //Preparado para execução

        if(task->fila_atual){     //Se estiver inserido em uma fila, ...
            queue_remove(task->fila_atual, (queue_t *) task);   //... remove-lo desta fila e...
        }
        
        queue_append((queue_t **) &fila_tprontas, (queue_t *) task);     //... inseri-lo na fila de prontos, ...
        task->fila_atual = (queue_t **) &fila_tprontas;    //... atualizando sua nova fila em seguida.

        return 0;
}

//Função interna para ajudar a executar e remove-la da fila que está inserida
//Retorna 0 caso ocorra tudo certo, -1 caso haja um erro
int task_set_executing(task_t* task){
    
        task->status = EXECUTANDO; //Em execução

        if(task->fila_atual) { //Se estiver inserido em uma fila, ...       
            queue_remove(task->fila_atual, (queue_t *) task);   //... remove-lo desta fila e...
            task->fila_atual = NULL; //... atualizar para fila nula em seguida.
        }

        return 0;
}


//==========================P4============================
// define a prioridade estática de uma tarefa (ou a tarefa atual)
void task_setprio (task_t *task, int prio) {

    if(!task){                  //Para uma tarefa nula, será alterado a tarefa em execução
        task = tarefa_atual;
        	} 

    if(prio > PRIO_MIN) {             //Prioridade mínima é 20
        prio = PRIO_MIN;
        			}

    else if(prio < PRIO_MAX){              //Prioridade máxima é -20
        prio = PRIO_MAX;
    			}

    task->prio_estat = prio;
    
    task_set_dinamic_prio(task,prio);
    
    #ifdef DEBUG
    printf("task_setprio: prioridade estática de %d agora é %d\n", task->id, task->prio_estat);
    #endif  //DEBUG
    
}

// retorna a prioridade estática de uma tarefa (ou a tarefa atual)
int task_getprio (task_t *task) {
        if(!task) {                  //Se for passado uma tarefa nula, utilize a tarefa em execução
        task = tarefa_atual;
        		}
    #ifdef DEBUG
    printf("task_getprio: prioridade estática de %d é %d\n", task->id, task->prio_estat);
    #endif  //DEBUG
    return task->prio_estat;  
}

//Envelhece todas as tarefas da fila exceto a passada por argumento
void task_get_old(task_t* task_excluded)
{
    //Existe um elemento?
    if(!task_excluded){
        return;
    }
    //Ele pertence a alguma fila?
    if(!task_excluded->next){
        return;
    }

    //Incremente em alpha a prioridade de todos os elementos exceto o primeiro
    for(task_t *it = task_excluded->next; it != task_excluded; it = it->next){
        int prio = task_get_dinamic_prio(it);
        if(between(PRIO_MAX,prio,PRIO_MIN)){
            task_alpha_dinamic_prio(it, ALPHA);
        }
    }
}

//Altera a prioridade dinâmica de um processo
void task_set_dinamic_prio(task_t* task, int prio)
{
    if(!task){                   //Para uma tarefa nula, será alterado a tarefa em execução
        task = tarefa_atual;
        	}

    if(prio > PRIO_MIN){               //Prioridade mínima é 20
        prio = PRIO_MIN;
    }
    else if(prio < PRIO_MAX){          //Prioridade máxima é -20
        prio = PRIO_MAX;
        	}
    task->prio_dinam = prio;

    #ifdef DEBUG
    printf("task_setdnprio: prioridade dinâmica de %d agora é %d\n", task->id, task->prio_dinam);
    #endif  //DEBUG
}

//Retorna prioridadade dinâmica de um processo
int task_get_dinamic_prio(task_t *task)
{
    if(!task){                   //Se for passado uma tarefa nula, utilize a tarefa em execução
        task = tarefa_atual;
    }
    #ifdef DEBUG
    printf("task_getdnprio: prioridade dinâmica de %d é %d\n", task->id, task->prio_dinam);
    #endif  //DEBUG
    return task->prio_dinam;
}

void task_alpha_dinamic_prio(task_t* task, int alpha)
{
    if(!task){                   //Para uma tarefa nula, será alterado a tarefa em execução
        task = tarefa_atual;
        	}
    int novo_prio_dinam = task->prio_dinam + alpha;
    if(novo_prio_dinam > PRIO_MIN){               //Prioridade mínima é 20
        task->prio_dinam = PRIO_MIN;
    }
    else if(novo_prio_dinam < PRIO_MAX){              //Prioridade máxima é -20
        task->prio_dinam = PRIO_MAX;
    }
    else{
        task->prio_dinam = novo_prio_dinam;
    }

    #ifdef DEBUG
    printf("task_incdnprio: prioridade dinâmica de %d agora é %d\n", task->id, task->prio_dinam);
    #endif  //DEBUG
}

//Retorna um numero positivo se task1 tem mais prioridade que task2,
//um numero negativo se task2 tem mais prioridade que task1
//e zero se task1 e task2 são iguais
int task_compare(task_t *task1, task_t *task2)
{
    return task_get_dinamic_prio(task2) - task_get_dinamic_prio(task1);
}

//Ordenação por counting sort da prioridade dinâmica
task_t* prioridade_max(task_t** task_q, int task_comp(task_t*,task_t*)){
    //Verificação da fila, se a mesma foi iniciada
    if(!task_q){
        return NULL;
    }

    //Verificação da fila, se a mesma não está vazia
    if(!*task_q){
        return NULL;
    }

    //Inicio counting sort (vetor da frequência de prioridade)
    //Obs.: por poder existir prioridades negativas, os índices devem ser normalizados para PRIO_MAX = 0 (ou prio - PRIO_MAX)
    /*int alcance = PRIO_MIN-PRIO_MAX;

    int aux_v[alcance];

    for(int i = 0; i < alcance; i++){
        aux_v[i] = 0;
    }
*/
    task_t *first = *task_q;
    task_t *max_prio = first;

    //aux_v[task_get_dinamic_prio(first)-PRIO_MAX]++;

    for(task_t *it = first->next; it != first; it = it->next){
        //aux_v[task_get_dinamic_prio(it)-PRIO_MAX]++;
        if(task_comp(max_prio, it) < 0){
            max_prio = it;
        }
    }
/*
    int sum = 0;

    for(int i = 0; i < alcance; i++){
        int a_sum = aux_v[i];
        aux_v[i] = sum;
        sum += a_sum;
    }

    int size = queue_size((queue_t*)(*task_q));

    task_t **srt_v = (task_t**)malloc(size*sizeof(task_t*));

    srt_v[aux_v[task_get_dinamic_prio(first)-PRIO_MAX]] = first;

    aux_v[task_get_dinamic_prio(first)-PRIO_MAX]++;

    for(task_t *it = first->next; it != first; it = it->next){

        srt_v[aux_v[task_get_dinamic_prio(it)-PRIO_MAX]] = it;

        aux_v[task_get_dinamic_prio(it)-PRIO_MAX]++;
    }

    //Fim do counting sort
    //Refazendo fila, na ordem inversa
    for(int i = 0; i < size; i++){
        //Indice da traseira/dianteira de um elemento
        int traseira = (i-1)%size;
        int dianteira = (i+1)%size;

        //Garante um indice entre 0 e size
        while(traseira < 0) traseira += size;
        while(dianteira < 0) dianteira += size;

        task_t *back = srt_v[traseira];
        task_t *front = srt_v[dianteira];

        srt_v[i]->prev = back;
        srt_v[i]->next = front;
    }

    *task_q = *srt_v;

    free(srt_v);
*/
    return max_prio;
}

//p05===============================================================
//Inicializa o temporizador do sistema
void init_timer_system(){
    //Definiçoes do signal da interrupcao
    action.sa_handler = timer_tick;             //funcao callback do signal trata manipulacao de ticks
    sigemptyset (&action.sa_mask);
    action.sa_flags = 0 ;

    if (sigaction (SIGALRM, &action, 0) < 0)    //Definicao do signal como SIGALRM gera uma interrupcao pelo temporizador
    {
        perror ("Erro em sigaction: ") ;
        exit (1) ;
    }

    timer.it_value.tv_usec = TICK_MSEG;      // primeiro disparo, em micro-segundos
    timer.it_value.tv_sec  = TICK_SEG;      // primeiro disparo, em segundos
    timer.it_interval.tv_usec = TICK_MSEG;   // disparos subsequentes, em micro-segundos
    timer.it_interval.tv_sec  = TICK_SEG;   // disparos subsequentes, em segundos

    if (setitimer (ITIMER_REAL, &timer, 0) < 0) //temprizador pelo tempo real
    {
        perror ("Erro em setitimer: ") ;
        exit (1) ;
    }
    #ifdef DEBUG
    printf("init_timer: temporizador iniciado com %d segundos mais %d microssegundos\n", TICK_SEG, TICK_MSEG);
    #endif  //DEBUG
}

// tratador do signal, manipula interupção a cada tick
void timer_tick(int signum){

    sys_clock_ms++;
    tarefa_atual->t_executado++;


    #if defined(DEBUG_ALL) || defined(DEBUG_OPERATIONAL_SYSTEM)
    printf("timer_tick: alarme %d tick %d de %d em %ums\n", signum, QUANTUM - quantum_count, QUANTUM, systime());
    printf("timer_tick: tarefa %d com %ums de processamento \n", tarefa_atual->id, tarefa_atual->t_executado);
    #endif  //defined(DEBUG_ALL)
        
    if(tarefa_atual->task_dono == USUARIO){
    
        if(!quantum_count--){
            #ifdef DEBUG
            printf("timer_tick: fim do quantum de %d, trocando para dispatcher\n", tarefa_atual->id);
           // printf("Tamanho do Quantum %d \n", quantum_count);
            #endif  //DEBUG
            task_switch(&dispatcher);
        }
    }
}
//p06=========================================================================
// retorna o relógio atual (em milisegundos)
 sys_clock_t systime(){
    return sys_clock_ms;
}