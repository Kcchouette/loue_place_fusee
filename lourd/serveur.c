#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>

#include <sys/types.h>
#include <sys/wait.h>

#include <signal.h>

#include "serveur.h"

int addition (reservation resa);

int getConsultation (reservation resa[], int taille, date_t date);

void insertion (tabReservation *tabResa, date_t date);

char* setReservation (tabReservation *tabResa, date_t date, int nbPlace);

void sig_handler (int signo);

int main()
{
    //signal Ctrl + C
    if (signal(SIGINT, sig_handler) == SIG_ERR)
        printf("\ncan't catch SIGINT\n");

    //structure
    reqConsultation_t reqConsultation;
    reqReservation_t reqReservation;

    ansReservation_t ansReservation;
    ansConsultation_t ansConsultation;

    //enfant
    pid_t pid;

    //wait
    int status;

    //shared memory
    tabReservation *mem;

    //Initialize Semaphore Buffers
    struct sembuf sb;

    //initialisation mémoire partagée
    if ((shmid = shmget(KEY_SHAREDMEMORY, sizeof(tabReservation), 0666 | IPC_CREAT | IPC_EXCL)) < 0)
    {
        perror("ERROR shmget");
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

    pid_t ret = fork();
    if(ret < 0)
    {
        perror("ERROR fork réservation");
        exit(EXIT_FAILURE);
    }
    else if (ret == 0)
    {
        if ((msqidReservation = msgget(RESERVATION_KEY, 0666 | IPC_CREAT | IPC_EXCL)) < 0)
        {
            perror("ERROR msgget réservation");
            exit(EXIT_FAILURE);
        }

        while(1)
        {
            if(msgrcv(msqidReservation, &reqReservation, sizeof(reqReservation_t) - sizeof(long), mtype_demandeReservation, 0) == -1)
            {
                perror("ERROR msgrcv réservation");
                exit(EXIT_FAILURE);
            }
            pid_t fils = fork();
            if(fils < 0)
            {
                perror("ERROR fork réservation");
                exit(EXIT_FAILURE);
            }
            else if (fils == 0)
            {
                //négatif
                sb.sem_num = 0;
                sb.sem_op = -1;
                sb.sem_flg = 0;
                //donc on lock
                if (semop(semid, &sb, 1) < 0)
                {
                    perror("ERROR semop decrementing réservation");
                    exit(EXIT_FAILURE);
                }

                if((mem = shmat(shmid, NULL, 0))==(tabReservation *)-1)
                {
                    perror("ERROR shmat réservation");
                    exit(EXIT_FAILURE);
                }

                char *msg = setReservation(mem, reqReservation.date, reqReservation.nbPlace);

                strcpy (ansReservation.message, msg);
                ansReservation.mtype = reqReservation.client;
                free(msg);

                //sleep(30); //pour tester les sémaphores

                //positif
                sb.sem_num = 0;
                sb.sem_op = 1;
                sb.sem_flg = 0;
                //donc on délocke
                if (semop(semid, &sb, 1) < 0)
                {
                    perror("ERROR semop incrementing réservation");
                    exit(EXIT_FAILURE);
                }

                if (msgsnd(msqidReservation, &ansReservation, sizeof(ansReservation_t) - sizeof(long), 0) < 0 )
                {
                    perror("ERROR msgsnd réservation");
                    exit(EXIT_FAILURE);
                }
                shmdt(mem);
            }
        }
    }
    else
    {
        if ((msqidConsultation = msgget(CONSULTATION_KEY, 0666 | IPC_CREAT | IPC_EXCL)) < 0)
        {
            perror("ERROR msgget consultation");
            exit(EXIT_FAILURE);
        }

        while(1)
        {
            if(msgrcv(msqidConsultation, &reqConsultation, sizeof(reqConsultation_t) - sizeof(long), mtype_demandeConsultation, 0) == -1)
            {
                perror("ERROR msgrcv consultation");
                exit(EXIT_FAILURE);
            }
            pid_t fils = fork();
            if(fils < 0)
            {
                perror("ERROR fork consultation");
                exit(EXIT_FAILURE);
            }
            else if(fils == 0)
            {
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

                if((mem = shmat(shmid, NULL, 0))==(tabReservation *)-1)
                {
                    perror("ERROR shmat consultation");
                    exit(EXIT_FAILURE);
                }

                ansConsultation.mtype = reqConsultation.client;
                ansConsultation.nbplace = getConsultation(mem->reservation, mem->taille, reqConsultation.date);

                if (msgsnd(msqidConsultation, &ansConsultation, sizeof(ansConsultation_t) - sizeof(long), 0) <0 )
                {
                    perror("ERROR msgsnd consultation");
                    exit(EXIT_FAILURE);
                }
                shmdt(mem);
            }
        }
    }

    return (EXIT_SUCCESS);
}

//Si on change le paramètre resa par fusee, il faut aussi envoyer la taille du paramètre fusée, car sizeof bug dans ce cas
int addition (reservation resa)
{
    int add = 0;
    for (int i = 0 ; i < sizeof(resa.fusee)/sizeof(int) ; ++i)
        add += resa.fusee[i];
    return add;
}

int getConsultation(reservation resa[], int taille, date_t date)
{
    for(int i = 0; i < taille; ++i)
    {
        if ((date.jour == resa[i].date.jour) && (date.mois == resa[i].date.mois) && (date.annee == resa[i].date.annee))
        {
            return addition(resa[i]);
        }
    }
    return addition(resa_default);
}

void insertion (tabReservation *tabResa, date_t date)
{
    tabResa->reservation[tabResa->taille].date = date;
    memcpy(tabResa->reservation[tabResa->taille].fusee, resa_default.fusee, sizeof(resa_default.fusee));
    tabResa->taille ++;
}

char* setReservation(tabReservation *tabResa, date_t date, int nbPlace)
{
    char *message = malloc(sizeof(char) * MSG_SIZE);

    for(int i = 0; i < tabResa->taille ; ++i)
    {
        if ((date.jour == tabResa->reservation[i].date.jour) && (date.mois == tabResa->reservation[i].date.mois) && (date.annee == tabResa->reservation[i].date.annee))
        {
            if(addition(tabResa->reservation[i]) >= nbPlace)
            {
                char tabMessage[5];

                strcpy(message, "Votre réservation a été effectuée dans la(les) fusée(s) :");

                for (int j = 0; j < sizeof(tabResa->reservation[i].fusee)/sizeof(int) ; ++j)
                {
                    if(tabResa->reservation[i].fusee[j] > 0 && nbPlace > 0)
                    {
                        sprintf(tabMessage, " %d", j + 1);
                        strcat(message, tabMessage);

                        if(tabResa->reservation[i].fusee[j] >= nbPlace)
                        {
                            tabResa->reservation[i].fusee[j] -= nbPlace;
                            nbPlace = 0;
                        }
                        else
                        {
                            nbPlace -= tabResa->reservation[i].fusee[j];
                            tabResa->reservation[i].fusee[j] = 0;
                        }
                    }
                }
                return (message);
            }
            else
            {
                strcpy(message, "Plus assez de place");
                return (message);
            }
        }
    }
    if (tabResa->taille < TAB_SIZE)
    {
        insertion(tabResa, date);
        message = setReservation(tabResa, date, nbPlace);
    }
    else
    {
        strcpy(message, "Désolé, le nombre maximum de réservation est atteint");
    }
    return (message);
}

void sig_handler(int signo)
{
    if (signo == SIGINT)
    {
        printf("Exiting\n");
        msgctl (msqidReservation, IPC_RMID, NULL);
        msgctl (msqidConsultation, IPC_RMID, NULL);
        //shmdt(sizeof(tabReservation));
        shmctl(shmid, IPC_RMID, NULL);
        semctl(semid, 0, IPC_RMID, 1);
        exit(EXIT_SUCCESS);
    }
}
