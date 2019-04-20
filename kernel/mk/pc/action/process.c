/*
 * File: pc/process.c 
 *
 * Descri��o:
 *     Gerenciamento de processos.
 *     PM - Process Manager (Parte fundamental do Kernel Base).
 *     Interfaces para o kernel chamar as rotinas de gerenciamento de
 * processos. 
 *     As rotinas aqui s�o independentes da arquitetura, quem se preocupa
 * com a arquitetura s�o as rotinas de inicializa��o comecadas por Ki.
 *     'Ki' para m�dulos internos do kernel.
 *
 * Atribui��es:
 *     Rotinas de gerenciamento das processos, cria��o, inicializa��o,
 * finaliza��o, task switch e outros.
 *
 *    Tipos de Processos:
 *
 *    PB - Processos inclu�dos no Kernel Base.
 *    P0 - Processos em ring0.
 *    P1 - Processos em ring1.
 *    P2 - Processos em ring2.
 *    P3 - Processos em ring3, User Mode.	
 *
 * @todo: 
 * Essas interfaces devem ser padronizadas, obedecendo roteiros de abertura, 
 * fechamento, salvamento e restaura��o de pilha ou argumento.
 * +Criar a fun��o get_current_pid. (obs: Isso j� existe)
 *
 * In this file:
 * ============
 *     +processSendSignal
 *     +create_process 
 *     +CloseAllProcesses
 *     +show_process_information
 *     +muitas outras rotinas...
 *
 * History:
 *     2015 - Create by Fred Nora.
 */
 
 
/*
    Fluxo padr�o:
	
	The Standard Streams.
	
	Every process is initialized with three open file descriptors, 
	stdin, stdout, and stderr. 
	stdin is an abstraction for accepting input (from the keyboard or from pipes) and 
	stdout is an abstraction for giving output (to a file, to a pipe, to a console).

    That's a very simplified explanation but true nonetheless. 
	Those three file descriptors are collectively called 'The Standard Streams'.	
    
	Standard input - this is the file handle that your process reads to 
	get information from you.
    Standard output - your process writes normal information to this file handle.
    Standard error - your process writes error information here.
	
    Quando um programa inicia, 
	o sistema operacional automaticamente define quem � 
	a entrada padr�o (geralmente o teclado) e quem � 
	a sa�da padr�o (geralmente a tela).
*/ 
 
 
#include <kernel.h>



//
// Fun��es importadas.
//

extern unsigned long get_page_dir (void);


//
// Vari�veis internas.
//

//int processmanagerStatus;
//int processmanagerError;
//...

int caller_process_id;

int processNewPID;   


//
//...
//

/*
 ***************************************
 * do_fork_process
 *     (Fun��o em desenvolvimento)
 *     Clona um processo sem thread.
 *     Clona o processo atual.
 *     Retorna o PID do clone.
 */
 
pid_t do_fork_process (void){
	
	int PID;
	
	struct process_d *Clone;
	struct process_d *Current;
	
	int Ret = -1;
	
	printf ("do_fork_process: Cloning the current process..\n");
	
	
	// ## Current ##
	// Checando a validade do processo atual.
	
	//if ( current_process < 0 )
	//    return 0;
	
	Current = (struct process_d *) processList[current_process];
	
	if ( (void *) Current == NULL )
	{
		printf("do_fork_process: current, struct \n");
		goto fail;
	
	}else{
		
		if ( Current->used != 1 || Current->magic != 1234 ){
		    
			printf("do_fork_process: current, validation \n");
			goto fail;		
		}
		
	    //#debug
	    //printf(">>> check current process: %d %d \n", current_process, Current->pid );		
		goto do_clone;
		//...
	};
	
	
	//
	// ## Clone ##
	//
	
do_clone:
	
	//Cria uma estrutura do tipo processo, mas n�o inicializada.
	
	Clone = (struct process_d *) processObject();
	
	if ( (void *) Clone == NULL )
	{
		printf("do_fork_process: Clone struct fail \n");
		
		goto fail;
	
	} else {
		
		
		// Obt�m um �ndice para um slot vazio na lista de processos.
		
	    PID = (int) getNewPID ();
		
		//if ( PID <= 0 ){
			
		if ( PID == -1 || PID == 0 )
		{	
			printf("do_fork_process: getNewPID fail %d \n", PID);
			goto fail;
		}
		
		Clone->pid = PID;
		
		Clone->used = 1;
		Clone->magic = 1234;
		
		//#obs: Na hora de copiar o processo, a estrutura do clone 
		//receber� os valores da estrutura do processo atual,
		//at� mesmo o endere�o do diret�rio de p�ginas.
		
		//...
		
		//Salvando na lista.
		
		processList[PID] = (unsigned long) Clone;
		
		//
		// ## clone  ##
		//
		
		// Clona efetivamente. 
		
		// #bugbug:
		// Essa rotina tem que ter um retorno, para falharmos 
		// caso ela falhe.
		
		Ret = processCopyProcess ( Current->pid, Clone->pid );
		
		if ( Ret != 0 )
		{
			printf("do_fork_process: processCopyProcess fail \n");
		    goto fail;	
		}
		    
		// Ok, retornando o n�mero do processo clonado.
		
		printf ("do_fork_process: done\n");
		
		return (pid_t) PID;	
	};

    // Fail.	
    	
fail:
    return (pid_t) -1;	
}


/*
 * processObject:
 *     Cria uma estrutura do tipo processo, mas n�o inicializada.
 *     #todo: Criar a mesma rotina para threads e janelas.
 */
 
struct process_d *processObject (void){
	
	struct process_d *p;
	
	p = (void *) malloc ( sizeof(struct process_d) );
	
	if ( (void *) p == NULL )
	{
        // #debug
	    printf ("processObject:");
		die ();
	};	
	
	return (struct process_d *) p;
}


/*
 * getNewPID:
 *     Pegar um slot vazio na lista de processos.
 *     +Isso pode ser usado para clonar um processo.
 */
 
pid_t getNewPID (void){
	
	struct process_d *p;
	
	
	// Come�aremos a busca onde come�a o range de IDs de processos de usu�rio.
	
	int i = USER_BASE_PID;
	
	while ( i < PROCESS_COUNT_MAX )
	{
		
	    p = (struct process_d *) processList[i];	
		
		// Se encontramos um slot vazio.
		// retornaremos o �ndice.
		
		if ( (void *) p == NULL )
		{	
			return (pid_t) i;
		}
		
		i++;
	};
	
	// Fail.
	
    return (pid_t) -1;	
}


/*
 * processTesting:
 *     Testando se o processo � v�lido. Se for v�lido retorna 1234.
 *     @todo: repensar os valores de retorno. 
 * system call (servi�o 88.)
 */

int processTesting (int pid){
	
	struct process_d *P;
	
	P = (void *) processList[pid];
	
	if ( (void *) P == NULL )
	{
		return (int) 0;
	}else{		
		
		if ( P->used == 1 && P->magic == 1234 ){
	        return (int) 1234;			
		}
	};
	
//fail:
	return 0;
}


/*
 * processSendSignal:
 *     Envia um sinal para um processo.
 *     Se o sinal e o processo forem v�lidos, um sinal � colocado
 * no PCB do processo.
 *     @todo: Rotinas envolvendo sinais devem ir para outro arquivo.
 */

int processSendSignal (struct process_d *p, unsigned long signal){
	
	//SIGNAL_COUNT_MAX
	
	//Limit
    //if(signal >= 32){
	//	return 1;
	//}
	
	if (signal == 0)
	{
		return 1;
	};
	
	//struct fail
	//if( (void*) p == NULL ){
	//	return 1;
	//}		
	
//ok:	
	//Ok
	if ( (void*) p != NULL )
	{	
		p->signal = (unsigned long) signal;
		return 0; //(int) signalSend(p,signal);
	}
	
	//...
	
//fail:
	
	return 1;
}


/*
 ****************************************
 * processCopyProcess
 *     copiar um processo.
 *     isso ser� usado por fork.
 */
 
int processCopyProcess ( pid_t p1, pid_t p2 ){
	
	int Status = 0;
	
    struct process_d *Process1;	
	struct process_d *Process2;
	
	if ( p1 == p2 ){
		goto fail;
	}
	
	//Check limits
	//if( p1 < 1 ...
	//if( p2 < 1 ...
	
	Process1 = (struct process_d *) processList[p1];
	Process2 = (struct process_d *) processList[p2];
	
	
	if ( (void *) Process1 == NULL )
	{
		printf("processCopyProcess: Process1\n");
		goto fail;
		
	}else{
		
		if( Process1->used != 1 || Process1->magic != 1234 )
		{
		   printf("processCopyProcess: Process1 used magic \n");
		   goto fail;			
		}	
	};
	
	if ( (void *) Process2 == NULL )
	{
		printf("processCopyProcess: Process1\n");
		goto fail;
	}else{

		if( Process2->used != 1 || Process2->magic != 1234 )
		{
		   printf("processCopyProcess: Process2 used magic \n");
		   goto fail;			
		}	
	};
	
	
	//copy
//copy:	

	Process2->objectType = Process1->objectType;
	Process2->objectClass = Process1->objectClass;	
	
	//Identificadores.
	Process2->pid  = (int) p2;       //PID.
    Process2->ppid = Process1->ppid; //PPID. 
	Process2->uid  = Process1->uid;  //UID. 
    Process2->gid  = Process1->gid;  //GID. 
		
	//State of process
	Process2->state = Process1->state;  
	
	//Plano de execu��o.
    Process2->plane = Process1->plane;		
		
	Process2->used = Process1->used;
	Process2->magic = Process1->magic;			
		
	//Process->name_address = NULL;
	
	Process2->framepoolListHead = Process1->framepoolListHead;
	
	//
	// * page directory address
	//
	
	Process2->DirectoryPA = Process1->DirectoryPA;
	
	Process2->Image = Process1->Image;
	
    //heap
	Process2->Heap = Process1->Heap;    
	Process2->HeapEnd = Process1->HeapEnd; 
	Process2->HeapSize = Process1->HeapSize;  	
	
	//stack
	Process2->Stack = Process1->Stack;   
	Process2->StackEnd = Process1->StackEnd; 
	Process2->StackSize = Process1->StackSize;   	
	Process2->StackOffset = Process1->StackOffset; 	
	
	
	Process2->iopl = Process1->iopl;
	
	
	
	Process2->base_priority = Process1->base_priority;
	Process2->priority = Process1->priority;	
	
	//?? herda a lista de threads ??
	Process2->threadListHead = Process1->threadListHead;
	
	Process2->zombieChildListHead = Process1->zombieChildListHead;
		
	Process2->exit_code = Process1->exit_code;
	
	Process2->dialog_address = Process1->dialog_address;
	
	//message support.
	Process2->window = Process1->window;  //arg1. 
	Process2->msg    = Process1->msg;     //arg2.
	Process2->long1  = Process1->long1;   //arg3.
	Process2->long2  = Process1->long2;   //arg4.		
	
	Process2->prev = Process1->prev; 		
	Process2->next = Process1->next; 

	Status = 0;
	goto done;	
	
fail:
	Status = 1;
    printf("processCopyProcess: fail:\n");
done:
    return (int) Status;	
};


/*
 ***********************************************************************
 * create_process:
 *     Cria process para ring 3.
 *     Preenche o PCB (Process Control Block).
 *
 * @todo: 
 *     Um processo na verdade inclui tudo sobre a tarefa,
 *     Desde o nome do arquivo at� o carregamento, registro e execu��o.
 *
 * Obs:
 *     Um job � um conjunto de processos.
 *     Tudo depende do tipo (funcionalidade): 
 *     SYSTEM, PERIODIC, ROUND_ROBIN, IDLE.
 *
 *    Classes de processos (tipo de n�vel):
 *
 *    PB - Processos inclu�dos no Kernel Base.
 *    P0 - Processos em ring0.
 *    P1 - Processos em ring1.
 *    P2 - Processos em ring2.
 *    P3 - Processos em ring3, User Mode.
 *
 *
 * @todo: 
 * Esse fun��o deve chamar uma rotina de cria��o de diret�rio. 
 * Quando um processo � criado deve-se criar o seu diret�rio e criar 
 * as pagetables que o processo vai usar de acordo com o tamanho do 
 * processo. 
 *
 * @todo: processCreateProcess(...)
 *
 * @todo: 
 * Aumetar o n�mero de argumentos para dar o suporte necess�rio para 
 * criar um processo do jeito que for necess�rio
 */

struct process_d *create_process ( struct room_d *room,
                                   struct desktop_d *desktop,
                                   struct window_d *window,
                                   unsigned long base_address, 
                                   unsigned long priority, 
                                   int ppid, 
                                   char *name, 
                                   unsigned long iopl,
                                   unsigned long directory_address )
{
  
	int i=0;
	
	pid_t PID;
	
    struct process_d *Process;

    // Para a entrada vazia no array de processos.
    struct process_d *Empty;      	

    // @todo:
    // Melhorar esse esquema de numera��o e 
	// contagem de processos criados.
	
    if ( processNewPID < USER_BASE_PID || processNewPID >= PROCESS_COUNT_MAX )
	{
		processNewPID = (int) USER_BASE_PID;	
	};
	
	PID = (int) processNewPID;
	
	Process = (void *) malloc ( sizeof(struct process_d) );
	
	if ( (void *) Process == NULL )
	{
	    printf("pc-process-create_process: Process");
		die();
		
		//@todo: Aqui pode retornar NULL.
	};


    //Loop.	
	// #BugBug: 
	// Isso pode virar um loop infinito.
	
get_next:
	
/*	
	i++;
	
	if ( i >= PROCESS_COUNT_MAX )
	{
		// #bugbug: 
		// Isso deixa o sistema devagar caso n�o apare�a 
		// a mensagem.
		
		printf("pc-process-create_process: End of list");
        refresh_screen();
		
		return NULL;
		//while(1){}
	};
	
*/	
	
	// Get empty.
		// Obt�m um �ndice para um slot vazio na lista de processos.
		
	PID = (int) getNewPID ();
			
	if ( PID == -1 || PID == 0 )
	{	
		printf("do_fork_process: getNewPID fail %d \n", PID);
		refresh_screen();
		return NULL;
	}
		
	Empty = (void *) processList[PID];
	
	//Se o slot estiver ocupado tentaremos o pr�ximo.
	//Na verdade podemos usar aquela fun��o que procura por um vazio. 
   	
	
    if ( (void *) Empty != NULL )
	{
		
		goto get_next;
		
	}else{
		
		//Object.
		Process->objectType = ObjectTypeProcess;
		Process->objectClass = ObjectClassKernelObjects;
		
		processNewPID = (int) PID;
		
		//Identificadores.
		Process->pid  = (int) PID;                  //PID.
        Process->ppid = (int) ppid;                 //PPID. 
		Process->uid  = (int) GetCurrentUserId();   //UID. 
        Process->gid  = (int) GetCurrentGroupId();  //GID. 

		//State of process
		Process->state = INITIALIZED;  

		//@TODO: ISSO DEVERIA VIR POR ARGUMENTO
        Process->plane = FOREGROUND;		
		
		//Error.
		//Process->error = 0;
		
		Process->used = 1;
		Process->magic = 1234;	
		
		//Name.
		//Process->name = name; //@todo: usar esse.
		//Process->cmd = NULL;  //nome curto que serve de comando.
		Process->name_address = (unsigned long) name;
		
		//lista de streams...
		//#todo temos que zerar essa lista e criarmos 3 streams para o processo.
		for ( i=0; i< NUMBER_OF_FILES; i++ ){
		    Process->Streams[i] = 0;
	    }
		
		//Process->terminal =
		
		//
	    //    ****  Banco de dados ****
		//
		
		//bancos de dados e contas do processo.
		//Process->kdb =
		//Process->gdbListHead =
		//Process->ldbListHead =
		//Process->aspaceSharedListHead =
		//Process->aspacePersonalListHead =
		//Process->dspaceSharedListHead =
		//Process->dspacePersonalListHead =
		
		// Inicializando a lista de framepools do processo.
		// @todo: Todo processo deve ser criado com pelo menos um 
		// frame pool, o que � equivalente a 4MB. (uma parti��o)
		// Obs: Um framepool indica onde � a �rea de mem�ria fisica
		// que ser� usada para mapeamento das p�ginas usadas pelo processo.
		
		Process->framepoolListHead = NULL;
		
		
		//Thread inicial.
		//Process->thread =
		
		//Process->processImageMemory =
		//Process->processHeapMemory =
		//Process->processStackMemory =
		
	    //
	    // ORDEM: O que segue � referenciado durante o processo de task switch.
	    //

		//
		// Page Directory: 
		//     Alocar um endere�o f�sico para o diret�rio de p�ginas do 
		// processo a ser criado, depois chamar a fun��o que cria o diret�rio.
		//
		// @todo:
		// *IMPORTANTE: Por enquanto os processos s�o criadas usando o 
		// diret�rio de p�ginas do processo Kernel. Mas temos que criar 
		// um diret�rio novo pra cada processo criado.
		// O diret�rio de todos os processos de usu�rio ser�o iguais. 
		// Ter�o uma �rea de us�rio particular e uma �rea compartilhada 
		// em kernel mode.
		//
		//@todo: Alocar um endere�o f�sico antes, depois chamar a fun��o que 
		// cria o pagedirectory.
		//@todo: 
        //op��o: KERNEL_PAGEDIRECTORY; //@todo: Usar um pra cada processo.

        if (directory_address == 0)
        {
			printf("create_process: page directory address fail\n");
			return NULL;
		}			
		
		//Process->Directory = (unsigned long ) directory_address; 
		
		Process->DirectoryVA = (unsigned long ) directory_address;
		Process->DirectoryPA = (unsigned long) virtual_to_physical ( directory_address, gKernelPageDirectoryAddress );
		
        //cancelados. 
		//Process->mmBlocks[32]
		//Process->mmblockList[32]
		
		
		//Process->processMemoryInfo
		
		
		// #todo: 
		// Precisa alocar espa�o na mem�ria f�sica.
        // Precisa criar page tables para essas areas de cada processo.		
		// Os endere�os virtuais dessas areas dos processos s�o sempre os mesmos.
		// mas os endere�os f�sicos dessas areas variam de processo pra processo.

		// Imagem do processo.
		// ?? Provavelmente esse endere�o � virtual.
		// Queremos que esse endere�o seja padronizado e que todos 
		// os processos usem o mesmo endere�o.
		
		// #bugbug
		// Todos os processos de usu�rio come�am no mesmo endere�o virtual.
		// Por�m temos os processos em kernel mode e os processos do gramado core
		// que usam endere�os virtuais diferentes.
		// #todo: Rever isso.
		// #todo: estamos suspendendo essa informa��o.
		
		//
		// ## IMPORTANTE ##
		//
		
		// Base da imagem do processo.

		
		//Na verdade precisamos aceitar o endere�o passado via argumento,
		//pois nem todos processos come�am no endere�o default.
		
		//UPROCESS_IMAGE_BASE;
		Process->Image = base_address;   
		
		// Tamanho da imagem do processo.
		// Temos que chamar a fun��o que pega o tamanho de um arquivo,
		// #bugbug: Porem, no momento o kernel n�o consegue ler arquivos
		// que est�o em subdiret�rios corretamente e os programas est�o em subdiret�rios.
		// #obs: O tamanho tamb�m poderia ser passado por arguemento.
		// #ou um argumento com ponteiro pra estrutura de informa��o sobre uma imagem.
		Process->ImageSize = 0;              	    
		
		//#todo: estrutura com informa��es sobre a imagem do processo.
		Process->image_info = NULL;
		
		// Heap e Stack:
		//
		// @todo: #BugBug 
		// O Heap e a Stack devem estar dentro da �rea de mem�ria do processo.
		// Uma pagetable do diret�rio � para o heap e outra para a stack.
        // Cada pagetable no diret�rio do processo � pra uma coisa.
        //
		// Obs: O endere�o virtual do heap e da stack dos processos ser�o 
		// os mesmos para todos os processos, assim como o endere�o virtual 
		// de carregamento da imagem.
		
		// Heap and Stack. 
		// #importante: (Endere�os virtuais).
		// Por isso pode ser o mesmo para todos os processos.
		
		
		// #### HEAP ####
		
		//CreatePageTable ( Process->DirectoryVA, 512, 0 );
		
		
		//Process->Heap = (unsigned long) 0x00400000; //funciona
		//Process->Heap = (unsigned long) 0xC0C00000; //funciona
		
		// g_heappool_va
        // endere�o virtual do pool de heaps.
        // os heaps nessa �rea ser�o dados para os processos.
		// base + (n*size)
		if (g_heap_count < 0 || g_heap_count >= g_heap_count_max)
		{
			//erro 
			printf("create_process: g_heap_count limits");
			die();
			//refresh_screen();
			//while(1){ asm ("hlt"); };
		}
		
		Process->Heap = (unsigned long) g_heappool_va + (g_heap_count * g_heap_size);
		Process->HeapSize = (unsigned long) g_heap_size;
		Process->HeapEnd = (unsigned long) (Process->Heap + Process->HeapSize); 
		
		g_heap_count++;
		
		//Process->Heap = (unsigned long) allocPages (64); 

        //Process->Heap = (unsigned long) malloc (1024*32); //32kb		
		
		// Endere�o do in�cio do Heap do processo.
		// #bubug: Endere�o do fim do heap.
		// Tamanho do heap, dado em KB.
	    //Process->Heap = UPROCESS_DEFAULT_HEAP_BASE;    
	    //Process->HeapEnd = 0; // @todo: (UPROCESS_DEFAULT_HEAP_BASE + UPROCESS_DEFAULT_HEAP_SIZE);
		//Process->HeapSize = (UPROCESS_DEFAULT_HEAP_SIZE/1024);    

		//Process->HeapPointer
		//Process->HeapLastValid
		//Process->HeapLastSize
	    
		// Endere�o do in�cio da Stack do processo.
		// Endere�o do fim da stack do processo.
		// Tamanho da pilha, dada em KB.
		// #importante: Deslocamento do endere�o do in�cio da pilha em rela��o 
		// ao in�cio do processo. 
		
		Process->Stack = UPROCESS_DEFAULT_STACK_BASE;   
	    Process->StackEnd = 0; // @todo: (UPROCESS_DEFAULT_STACK_BASE+UPROCESS_DEFAULT_STACK_SIZE);
		Process->StackSize = (UPROCESS_DEFAULT_STACK_SIZE/1024);   	
	    Process->StackOffset = UPROCESS_DEFAULT_STACK_OFFSET; 
	    

		//ring.
		Process->iopl = iopl; 
        
		
	    //PPL - (Process Permition Level).(gdef.h)
        //Determina as camadas de software que um processo ter� acesso irrestrito.
	    //Process->ppl = pplK0;
		

		
	    //Process->callerq	   //head of list of procs wishing to send.
        //Process->sendlink;    //link to next proc wishing to send.
        //Process->message_bufffer		   //pointer to message buffer.
        //Process->getfrom_pid		       //from whom does process want to receive.
        //Process->sendto_pid	               //pra quem.

	    //Signal
	    //Process->signal = 0;
	    //Process->signalMask = 0;
		
		//cancelada.
		//Process->process_message_queue[8]
		
		//Outras coisas.
		
		//Prioridade.
		Process->base_priority = (unsigned long) priority;
		Process->priority = (unsigned long)  Process->base_priority;
		
	    //Que tipo de scheduler o processo utiliza. (rr, realtime ...).
	    //Process->scheduler_type = ;   		
		
		
		//Process->step
		//Process->quantum
		//Process->timeout
		//Process->ticks_remaining
		
		//Process->ThreadQuantum   //As threads do processo iniciam com esse quantum.
		
		
		//Process->threadCount = 0;    //N�mero de threads do processo.
		
		//Process->tList[32] 
		
		//Lista de threads.
		Process->threadListHead = NULL;
		
		Process->control = NULL;

        //Process->window_id 		
		
		
		//Process->event
		
	
        // #importante
		// user session, room and desktop.
		
		// #bugbug: 
		// N�o temos informa��o sobre a user session, 
		// devemos pegar a estrutura de current user session. 
		// Para isso ela deve ser configurada na inicializa��o do gws,
		// antes da cria��o dos processo.
		
		Process->usession = CurrentUserSession;  // Current.
		Process->room = room;                    // Passado via argumento.
		Process->desktop = desktop;              // Passado via argumento.
		
		//Process->base_priority
		
		
		
		
	    // wait4pid: 
        // O processo esta esperando um processo filho fechar.
	    // Esse � o PID do processo que ele est� esperando fechar.
		
		Process->wait4pid = (pid_t) 0;
		
		Process->zombieChildListHead = NULL;
		
		Process->exit_code = 0;
		

		//procedimento eem ring 0 por enquanto.
		Process->dialog_address = (unsigned long) &system_procedure;

		Process->signal = 0;
		Process->signal_mask = 0;
		
		//Msg support.
		//Argumentos do procedimento de janela.
		//@todo: Isso pode ser um ponteiro de estrutura,
		//a fila de mensgens pode ser uma fila de ponteiros.
		Process->window = NULL;    //arg1. 
	    Process->msg = 0;          //arg2.
	    Process->long1 = 0;        //arg3.
	    Process->long2 = 0;        //arg4.			
		
		Process->prev = NULL; 
		Process->next = NULL; 

		//Coloca o processo criado na lista de processos.
		processList[PID] = (unsigned long) Process;		
		//Nothing.
	};	

	
	//
	// ?? O que falta mais aqui. ??
	//
	
    // ...	
	
//Done.
done:
    return (void *) Process;
}

 
/*
 * CloseAllProcesses:
 *     Bloqueia todos os processos da lista de processos.
 *     Menos o processo '0'.
 *     processCloseAllProcesses();    
 */

void CloseAllProcesses (void){
	
	int Index = 0;
    struct process_d *P;

	// #importante:
	// Menos o 0, pois � o kernel. 
    
	for ( Index = 1; Index < PROCESS_COUNT_MAX; Index++ )
	{
		//Pega, bloqueia e tira da lista.
		P = (void*) processList[Index];
		P->state = PROCESS_BLOCKED;
        processList[Index] = (unsigned long) 0;		
	};		
	
	//Check process 0.
	P = (void *) processList[0];
	
	if ( (void *) P == NULL )
	{
		panic("CloseAllProcesses: P\n");
	};
}


/*
 * KeSaveCurrentContext:
 *     Interface para chamar rotina de salvamento de contexto de registradores.
 * obs: N�o usaremos inicial Ke_
 */

void KeSaveCurrentContext (void)
{
   //sendo uma interface pode chamar a rotina de acordo com a arquitetura presente.
   //para isso pode-se usar hal.
    //return;
}


/*
 * KeRestoreCurrentContext:
 *     Interface para chamar rotina de restaura��o de contexto de registradores.
 * obs: N�o usaremos inicial Ke_
 */

void KeRestoreCurrentContext (void)
{
   //sendo uma interface pode chamar a rotina de acordo com a arquitetura presente.
   //para isso pode-se usar hal.
    //return;
}
 
 
/*
 * KeCpuFaults:
 *     Interface para chamar a rotina de tratamento das fault de CPU.
 *     @todo: Talvez essa rotina possa ir para cpu.c
 *  * obs: N�o usaremos inicial Ke_
 */ 

void KeCpuFaults (unsigned long fault_number){ 
		
	KiCpuFaults (fault_number);
}



/*
 * KeSpawnTask:
 *     Interface para rotina que executa pela primeira vez
 * uma tarefa em user mode.
 * @todo: deletar.  * obs: N�o usaremos inicial Ke_
 */
void KeSpawnTask(int id)
{
    KiSpawnTask(id);
	//no return.
};




/*
 * KeSelectNextThread:
 *     Interface para chamar a rotina que seleciona a proxima tarefa para
 * rodar, baseado em criterios.
 * * obs: N�o usaremos inicial Ke_
 */
int KeSelectNextThread(int current)
{
    
	//@todo: analizar o retorno.
	return (int) KiSelectNextThread(current);
}


/*
 * KeCheckTaskContext:
 *     Interface para chamar uma rotina de an�lise de contexto.
  * obs: N�o usaremos inicial Ke_
 */
int KeCheckTaskContext( int task_id )
{
	//#bugbug Aqui estamos chamando uma rotina detro do 
	//kernel base, n�o � essa a ideia.
	// e se alguma rotina dentro do kernel estava chamando essa 
	// fun��o � bom substituir a chamada por KiCheckTaskContext
    //@todo: prepara��o antes de chamar.filtros.
	return (int) KiCheckTaskContext(task_id);
};



/*
 * KeExecProcess:
 *     Interface para chamar a rotina que executa uma tarefa, dado
 * nome e endere�o.
 */

int KeExecProcess (int pid, unsigned long *process_address)
{
    return 0;
}


/*
 * KeDispatchProcess:
 *     Interface para chamr uma rotina para despachar uma thread.
 *     Server em user mode.
 */

void KeDispatchProcess (void)
{
    //return;
}


void KeDoProcessReady (int pid)
{
    //return;
}


void KeDoProcessRunning (int pid)
{
    //return;
}


void KeDoProcessSleeping (int pid)
{
    //return;
}


void KeDoProcessZombie (int pid)
{
    //return;
}


void KeDoProcessDead (int pid)
{
    //return;
}


/*
 * KeStartTask:
 *     Interface para chamar uma rotina de execu��o de um processo,
 * dado seu id e seu endere�o.
 */

void KeStartProcess ( unsigned long pid, unsigned long *task_address )
{
    //return;
}

 
/*
 * KeAcordarTarefa:
 *     Interface para chamar uma rotina de acordar um processo..
 */ 

void KeWakeupProcess (int pid)
{
    //return;
}
 
  
/*
 * KeProcessExit:
 *     Interface para chamar uma rotina de 
 *     tratamento de fechamento de um processo.
 *     Chama um m�dulo externo para sair de uma tarefa.
 *     Chama um Server em user mode.
 */ 

void KeProcessExit (void)
{
    //return;
}



/*
 * KeKillProcess:
 *     Chama um server em user mode pra fechar um processo.
 */ 

void KeKillProcess (int pid)
{
    //return;
}


// ??
void KeNewProcessScheduler (void)
{
    //return; 
}


int KeSetPriority (void)
{
    return 0;
}


//Aumenta a prioridade de um processo.
int KeIncreasePriority (int pid)
{
    return 0;
}


void KeSaveContextOfNewTask ( int id, unsigned long *task_address )
{
    //return;
}


void KeSetQuantum ( unsigned long q)
{
    //return;
}


unsigned long KeGetQuantum (void)
{ 
    return 0; 
}


void KeSetCurrentQuantum (unsigned long q)
{
    //return;
}


unsigned long KeGetCurrentQuantum (void)
{ 
    return 0; 
};


void KeSetNextQuantum (unsigned long q)
{
    //return;
}


unsigned long KeGetNextQuantum (void)
{ 
    return 0; 
}


//@todo: N�o faz sentido chamar um m�dulo externo
//para setar o foco.
// Isso � para tratamento de janela, deveria ir para kgws

void KeSetFocus (int pid)
{
    //return;
}


//@todo: 
// N�o faz sentido chamar um m�dulo externo
// para pegar o foco.
// Isso � para tratamento de janela, deveria ir para kgws

int KeGetFocus (void)
{ 
    return (int) 0; 
}


void KeShowTasksParameters (void)
{
    //return;
}


void KeShowPreemptedTask (void)
{
    //return;
}


void show_currentprocess_info (void){
	
    struct process_d *Current;		
	
	if( current_process < 0 || 
	    current_process >= PROCESS_COUNT_MAX )
	{
		//printf("show_process_information: current_process fail\n");
		return;
	};

	//Struct.
	Current = (void *) processList[current_process];
	
	if ( (void *) Current == NULL )
	{
	    printf ("show_process_information: struct \n");
        return; 		
	
	} else {
		
		//Index.
		printf ("PID={%d} PPID={%d} UID={%d} GID={%d} \n",
		    Current->pid, Current->ppid, Current->uid, Current->gid );
	    //Name
		printf("Name={%s} \n", Current->name_address );
		
		//Image Address.
		printf("ImageAddress={%x} \n", Current->Image );
		
		//Directory Address. *IMPORTANTE.
		printf (">>DirectoryPA={%x} \n", Current->DirectoryPA );		
		printf (">>DirectoryVA={%x} \n", Current->DirectoryVA );
		//Heap and stack.
		printf("Heap={%x}  HeapSize={%d KB}  \n", Current->Heap, 
		    Current->HeapSize );
												  
		printf("Stack={%x} StackSize={%d KB} \n", Current->Stack, 
		    Current->StackSize );
		
		//...
	};
	
	refresh_screen ();
}


/*
 * show_process_information:
 *     Mostra informa��es sobre os processos. 
 *     #todo: na verdade um aplicativo em user mode deve fazer esse trabalho
 * solicitando informa��es sobre cada processo atrav�s de chamadas.
 */

// Mostrar informa��es sobre os processos da lista.
// obs: as imagens s�o carregadas em endere�os virtuais diferentes
// e o endere�o mostrado � em rela��o ao diret�rio de p�ginas do kernel
// pois o kernel � que controla o posicionamento das imagens.

void show_process_information (void){
	
	int i=0;
	
	struct process_d *p;	
    
	printf ("show_process_information: \n");
	
	for ( i=0; i<PROCESS_COUNT_MAX; i++ )
    {
	    p = (void *) processList[i];
		
		if ( (void *) p != NULL && 
		           p->used == 1 && 
				   p->magic == 1234 )
	    { 
            
            // #bugbug
            // #todo Change that thing.
            
			printf ("PID=%d PPID=%d State=%d Base=%x Size=%d DirPA=%x DirVA=%x iopl=%d prio=%d wait4pid=%d Name={%s}\n\n", 
				p->pid, 
			    p->ppid,
				p->state,
				p->Image,
				p->ImageSize,	
				p->DirectoryPA,
				p->DirectoryVA,	
				p->iopl,
				p->priority,
				p->wait4pid,	
				p->name_address );
	    }
		//Nothing.
    };	
	
    //printf ("done\n");
	refresh_screen ();
}


/*
 **************************************************************
 * SetProcessDirectory:
 *     Configura o endere�o do diret�rio de p�ginas do processo.
 *     @todo: Isso pode ser um servi�o oferecido pelo kernel,
 * para um gerenciador de processos em user mode usar.
 * @todo: processSetDirectory(...)
 */

void SetProcessDirectory ( struct process_d *process, unsigned long Address ){
	
    if ( (void *) process != NULL )
	{
        process->DirectoryPA = (unsigned long) Address;        
	};
}


/*
 ************************************************************
 * GetProcessDirectory:
 *     Pega o endere�o do diret�rio de p�ginas do processo.
 *     @todo: Isso pode ser um servi�o oferecido pelo kernel,
 * para um gerenciador de processos em user mode usar.
 * @todo: processGetDirectory(...)
 */

unsigned long GetProcessDirectory( struct process_d *process )
{
    if( (void*) process != NULL )
	{
		//@todo: checar used e magic.
        return (unsigned long) process->DirectoryPA;
	};
	return (unsigned long) 0;
};


/*
 *********************************************************
 * GetPageDirValue:
 *     Pega o endere�o do diret�rio de p�ginas do processo.
 *     processGetPageDirValue()
 */

unsigned long GetPageDirValue (void)
{
    return (unsigned long ) get_page_dir ();
}



/*
 ***************************************************************
 * init_task:
 *     Inicia um processo.
 *     @todo: Mudar o nome para init_process().
 *            Inicializa a estrutura de um processo criado.
 *            Quando cria o processo, cria apenas sua estrutura,
 *            ficando a cargo dessa rotina inicializar a estrutura criada. 
 */ 

int init_task (int id)
{ 
    //@todo: inicializar uma estrutura para um processo criado.
	//obs: ja tem essa rotina feita em aalgum lugar..
	
    return 0;   
}



/*
 *************************************************************
 * init_tasks: 
 *     Inicia as vari�veis.
 *
 * @todo: 
 *     Bug bug, problemas na inicializa��o das estruturas.
 *     Implementar a aloca��o din�mica na cria��o das tarefas.
 */

//#bugbug
//rever e deletar, se poss'ivel.

//#bugbug
//os conceitos de tasks ainda est�o misturados, hora � thread e hora � processo

void init_tasks (void)
{
    init_processes ();	
}


/*
 ***********************************************************
 * init_processes:
 *    Inicaliza o process manager.
 *   #todo: rever esse nome, pois na verdade estamos inicializando variaveis 
 * usadas no gerenciamento de processo.
 */
 
void init_processes (void){
	
    int i;	
	
	//
	// Iniciando vari�veis globais.
	//
	
	kernel_request = 0;    // O que fazer com a tarefa atual.
	
	
	// ?? Contagem de tempo de execu��o da tarefa atual.	
	//n�o precisa, isso � atualizado pelo request()
	//kernel_tick = 0;                                 
	
	kernel_switch = 0;     // ?? Ativa o kernel switch do scheduler.
    
	current_process = 0;
	
	//
	// Inicializando a lista de processos.
	//
	
	i=0;
    
	while (i < PROCESS_COUNT_MAX)
    {
	    processList[i] = (unsigned long) 0;
        i++;
	};


	//
	// @todo: mais algo??
	//
	
//Done:	
	//return;
}


/*
 **********************************************************************
 * exit_process:
 *
 *     Exit process.
 *     Coloca o processo no estado PROCESS_TERMINATED, mas n�o destr�i a 
 * estrutura do processo. Outra rotina destruir� as informa��es.
 *
 * @todo: 
 * Liberar a mem�ria e os recursos usado pelo processo. Ou ainda apenas 
 * sinalizar a flag magic para que o GC a reutilize.
 *
 * @todo: 
 * Fechar as threads seguindo a lista encadeada.
 */

void exit_process ( pid_t pid, int code ){
	
	int i;
    struct process_d *Process;
	
    struct thread_d *Threads;
	struct thread_d *Next;	
	//...

	// N�o fechar o processo 0. Ele � o kernel.
	if ( pid == 0 ){
		return;
	}
	
	//Limits. 
	if ( pid < 0 || pid >= PROCESS_COUNT_MAX ){
	    return;	
	}
	
	// Mais limites ??

#ifdef MK_VERBOSE	
	//Debug:
	printf ("exit_process: Terminating process %d\n", pid );
	refresh_screen();
#endif	
	

	// Pega o ponteiro para a estrutura, muda o c�digo de sa�da 
	// e o status.
	
	Process = (void *) processList[pid];
	
	if( (void *) Process == NULL )
	{
		//printf ("Invalid PID\n");
		return;
	
	}else{	

		// Se estiver corrompida.
        if( Process->used != 1 || 
		    Process->magic != PROCESS_MAGIC )
		{
			return;
		};
		
		Process->exit_code = (int) code;    
		Process->state = PROCESS_TERMINATED; 
		//...
	};
		
#ifdef MK_VERBOSE
	//Debug:
	printf("exit_process: Terminating threads..\n");
	refresh_screen();
#endif		

	// Agora temos que terminar as threads que est�o na lista 
	// de threads do processo.
	// Pegaremos a primeira da lista.
	// Se o head da list n�o foi inicializado corretamente 
	// d� page fault.
	
	Thread = (void *) Process->threadListHead;
		
	// Se n�o h� nada na head.	
	if( Thread == NULL )
	{
		// @todo: Talvez haja mais o que fazer.
	    goto done;	
	}else{
		
		//used, magic ??
	};	
		
	
	// Se a primeira thread da lista � v�lida, ent�o tentaremos
	// fechar toda a lista.
	
	while (1)
	{
		// ?? Qual deve fechar depois. ??
		
		printf(".\n");
		refresh_screen();
		
		// Salva o ponteiro para o pr�ximo thread.
		Next = (void*) Thread->Next;
		
		// Confere se chegamos ao fim da lista.
		// 'Thread' fecha agora.
		
        if( Thread == NULL )
		{
		    goto done;	
		
		}else{
    
#ifdef MK_VERBOSE    
		    //fecha a thread.
		    printf ("exit_process: Killing thread %d.\n", Thread->tid );
#endif			
			
			// Kill !
			
			kill_thread ( Thread->tid );  					
		    
			// Prepara qual deve fechar agora.
		    // Hav�amos salvo e agora � vez dela.
		    // Obs: Estamos reusando o ponteiro.
			
			Thread = (void *) Next;
		 };
        //Nothing.
	};		
	//nothing
done:
	
	
	//@todo:
	//    Escalonar o processo atual. Se o processo fechado foi o processo 
	// atual, precisamos de um novo processo atual. Usaremos o processo zero 
	// por enquanto. Obs: Devemos fazer aqui apenas escalonameto de processo
	// e n�o de thread.	

	//Zerando por enquanto.

	//?? Analizar essa parte.	
    //@todo: Select next process (idle)
	
	current_process = 0;	
    current_thread = 0;    //@todo: Deletar isso.	
	

	//Process->used = 0;
	//Process->magic = 0;
	//Process = NULL;
	
	
	//#todo: chamar o scheduler.
	//scheduler ();
	return;
}


// ??
int get_caller_process_id (void)
{
	return (int) caller_process_id;
}


// ??
void set_caller_process_id (int pid)
{
	caller_process_id = (int) pid;
}


/*
 *************************************************
 * init_process_manager:
 *     Initialize process manager.
 *     processInitializeProcessManager();
 */

int init_process_manager (void){
	
	caller_process_id = (int) 0;
	
	processNewPID = (int) USER_BASE_PID;
	
	//...
		
	return 0;
}


/*
 *Constructor.
int processmanagerProcessmanager(){
	;
};
*/


/*
int processmanagerInit(){
	;
};
*/


/*
 * GetProcessHeapStart:
 *     Pega o endere�o do heap do processo.
 */

unsigned long GetProcessHeapStart ( pid_t pid ){
	
	struct process_d *process;
	
	//Limits.
	
	if ( pid < 0 || pid >= PROCESS_COUNT_MAX )
	{
		goto fail; 
	}
	
	process = (struct process_d *) processList[pid];
    
	if ( (void *) process == NULL )
	{
		goto fail;
		 
	}else{
		
		if ( process->used != 1 || process->magic != 1234 )
		{
			goto fail;
		}
		
		//Ok.
		return (unsigned long) process->Heap;
	};
	
fail:	
    return (unsigned long) 0;
}


/*
 * GetProcessPageDirectoryAddress:
 *
 */

unsigned long GetProcessPageDirectoryAddress ( pid_t pid ){
	
	struct process_d *process;
	
	//Limits.
	
	if( pid < 0 || pid >= PROCESS_COUNT_MAX )
	{
		goto fail; 
	};
	
	
	process = (struct process_d *) processList[pid];
    
	if ( (void *) process == NULL )
	{
		goto fail;
		 
	}else{
		
		if ( process->used != 1 || process->magic != 1234 ){
			goto fail;
		}
		
		return (unsigned long) process->DirectoryPA;
	};
	
fail:	
    return (unsigned long) 0;
}


//#todo
//encontrar um slot vazio em Process->Streams[]
//retornar o indice.
/*
int process_find_empty_stream_slot ( struct process_d *process );
int process_find_empty_stream_slot ( struct process_d *process ){

 	int i = 0;
	
	if ( (void *) process == NULL )
	{
		return -1;
		 
	}else{
		
		if ( process->used != 1 || process->magic != 1234 ){
			
			return -1;
		}
		
	    for ( i=0; i< NUMBER_OF_FILES; i++ )
	    {
	        if ( Process->Streams[i] == 0 )
		    {
		        return i;
		    }
	    }		
	};  
	
	return -1;
} 
*/

//
// End.
//
