#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/msg.h>
#include <sys/sem.h>

#include <sys/types.h>
#include <sys/wait.h>

#include <pthread.h>

#include <signal.h>

#include "serveur.h"

int addition (reservation *resa);

int getConsultation(reservation *head, date_t date);

void insertion (reservation **head, date_t date);

char* setReservation(reservation **head, date_t date, int nbPlace);

void sig_handler(int signo);

void* consultation (void *arg);

void* creerReservation (void *arg);

void* reserve (void *arg);

//Initialize Semaphore Buffers
struct sembuf sb;

//chaîne réservation
reservation *head, *current;

int main()
{
    //signal Ctrl + C
    if (signal(SIGINT, sig_handler) == SIG_ERR)
        printf("\ncan't catch SIGINT\n");

    head = NULL;

    pthread_t reserv, consult;

    if(pthread_create(&reserv, NULL, &reserve, NULL) != 0)
    {
        perror("ERROR semget reservation");
        exit(EXIT_FAILURE);
    }

    //initialisation sémaphore
    //1 est le nombre de sémaphore
    if ((semid = semget(KEY_SEMAPHORE, 1, 0666 | IPC_CREAT | IPC_EXCL)) < 0)
    {
        perror("ERROR semget");
        exit(EXIT_FAILURE);
    }

    if(semctl(semid, 0, SETVAL, 1) < 0)
    {
        perror("ERROR semctl");
        exit(EXIT_FAILURE);
    }

    if ((msqidConsultation = msgget(CONSULTATION_KEY, 0666 | IPC_CREAT | IPC_EXCL)) < 0)
    {
        perror("ERROR msgget consultation");
        exit(EXIT_FAILURE);
    }

    reqConsultation_t reqConsultation;
    while(1)
    {
        if(msgrcv(msqidConsultation, &reqConsultation, sizeof(reqConsultation_t) - sizeof(long), mtype_demandeConsultation, 0) == -1)
        {
            perror("ERROR msgrcv consultation");
            exit(EXIT_FAILURE);
        }

        reqConsultation_t *arg = malloc(sizeof *arg);
        arg->client = reqConsultation.client;
        arg->date = reqConsultation.date;
        arg->mtype = reqConsultation.mtype;
        if(pthread_create(&consult, NULL, &consultation, arg) != 0)
        {
            perror("ERROR semget :");
            exit(EXIT_FAILURE);
        }
        pthread_join(consult, NULL);

    }
    pthread_join(reserv, NULL);
    return (EXIT_SUCCESS);
}

//Si on change le paramètre resa par fusee, il faut aussi envoyer la taille du paramètre fusée, car sizeof bug dans ce cas
int addition (reservation *resa)
{
    int add = 0;
    for (int i = 0 ; i < sizeof(resa->fusee)/sizeof(int) ; ++i)
        add += resa->fusee[i];
    return add;
}

int getConsultation(reservation *head, date_t date)
{
    reservation *resa = head;

    while(resa)
    {
        if ((date.jour == resa->date.jour) && (date.mois == resa->date.mois) && (date.annee == resa->date.annee))
        {
            return addition(resa);
        }
        resa = resa->resa_suivante;
    }
    return addition(&resa_default);
}

void insertion (reservation **head, date_t date)
{
    reservation *current;
    current = (reservation *)malloc(sizeof(reservation));
    current->date = date;
    memcpy(current->fusee, resa_default.fusee, sizeof(resa_default.fusee));
    current->resa_suivante = *head;
    *head = current;
}

char* setReservation(reservation **head, date_t date, int nbPlace)
{
    reservation *resa = *head;
    char *message = malloc(sizeof(char) * MSG_SIZE);

    while(resa)
    {
        if ((date.jour == resa->date.jour) && (date.mois == resa->date.mois) && (date.annee == resa->date.annee))
        {
            if(addition(resa) >= nbPlace)
            {
                char tabMessage[5];

                strcpy(message, "Votre réservation a été effectuée dans la(les) fusée(s) :");

                for (int j = 0; j < sizeof(resa->fusee)/sizeof(int) ; ++j)
                {
                    if(resa->fusee[j] > 0 && nbPlace > 0)
                    {
                        sprintf(tabMessage, " %d", j + 1);
                        strcat(message, tabMessage);

                        if(resa->fusee[j] >= nbPlace)
                        {
                            resa->fusee[j] -= nbPlace;
                            nbPlace = 0;
                        }
                        else
                        {
                            nbPlace -= resa->fusee[j];
                            resa->fusee[j] = 0;
                        }
                    }
                }
                return (message);
            }
            else
            {
                strcpy(message, "Plus assez de place pour ce jour");
                return (message);
            }
        }
    }
    insertion(head, date);
    message = setReservation(head, date, nbPlace);
    return(message);
}

void sig_handler(int signo)
{
    if (signo == SIGINT)
    {
        printf("Exiting\n");
        msgctl (msqidReservation, IPC_RMID, NULL);
        msgctl (msqidConsultation, IPC_RMID, NULL);
        semctl(semid, 0, IPC_RMID, 1);
        exit(EXIT_SUCCESS);
    }
}

void* consultation (void *arg)
{
    reqConsultation_t *reqConsul = arg;
    //négatif
    sb.sem_num = 0;
    sb.sem_op = -1;
    sb.sem_flg = 0;
    //donc on lock
    if (semop(semid, &sb, 1) < 0)
    {
        perror("ERROR semop decrementing consultation");
        exit(EXIT_FAILURE);
    }

    //positif
    sb.sem_num = 0;
    sb.sem_op = 1;
    sb.sem_flg = 0;
    //donc on délocke
    if (semop(semid, &sb, 1) < 0)
    {
        perror("ERROR semop incrementing consultation");
        exit(EXIT_FAILURE);
    }

    ansConsultation_t ansConsultation;


    ansConsultation.mtype = reqConsul->client;
    ansConsultation.nbplace = getConsultation(head, reqConsul->date);

    if (msgsnd(msqidConsultation, &ansConsultation, sizeof(ansConsultation_t) - sizeof(long), 0) <0 )
    {
        perror("ERROR msgsnd consultation");
        exit(EXIT_FAILURE);
    }
    free(reqConsul);
    pthread_exit(NULL);
}

void* creerReservation (void *arg)
{
    reqReservation_t *reqReserv = arg;

    sb.sem_num = 0;
    sb.sem_op = -1;
    sb.sem_flg = 0;
    if (semop(semid, &sb, 1) < 0)
    {
        perror("ERROR semop decrementing réservation");
        exit(EXIT_FAILURE);
    }

    ansReservation_t ansReservation;
    char *msg = setReservation(&head, reqReserv->date, reqReserv->nbPlace);
    strcpy (ansReservation.message, msg);
    ansReservation.mtype = reqReserv->client;
    free(msg);

    sb.sem_num = 0;
    sb.sem_op = 1;
    sb.sem_flg = 0;
    if (semop(semid, &sb, 1) == -1)
    {
        perror("ERROR semop incrementing réservation");
        exit(EXIT_FAILURE);
    }

    if (msgsnd(msqidReservation, &ansReservation, sizeof(ansReservation_t) - sizeof(long), 0) < 0 )
    {
        perror("ERROR msgsnd réservation");
        exit(EXIT_FAILURE);
    }
    free(reqReserv);
    pthread_exit(NULL);
}

void* reserve (void *arg)
{
    if ((msqidReservation = msgget(RESERVATION_KEY, 0666 | IPC_CREAT | IPC_EXCL)) < 0)
    {
        perror("ERROR msgget réservation");
        exit(EXIT_FAILURE);
    }

    reqReservation_t reqReservation;
    ansReservation_t ansReservation;

    pthread_t creerReserv;

    while(1)
    {
        if(msgrcv(msqidReservation, &reqReservation, sizeof(reqReservation_t) - sizeof(long), mtype_demandeReservation, 0) == -1)
        {
            perror("ERROR msgrcv réservation");
            exit(EXIT_FAILURE);
        }

        reqReservation_t *arg = malloc(sizeof *arg);
        arg->client = reqReservation.client;
        arg->date = reqReservation.date;
        arg->mtype = reqReservation.mtype;
        arg->nbPlace = reqReservation.nbPlace;

        if(pthread_create(&creerReserv, NULL, &creerReservation, arg) != 0)
        {
            perror("ERROR semget reservation");
            exit(EXIT_FAILURE);
        }
        pthread_join(creerReserv, NULL);
    }
    pthread_exit(NULL);
}
