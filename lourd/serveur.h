#ifndef SERVEUR_H_INCLUDED
#define SERVEUR_H_INCLUDED

#include <unistd.h>
#include <errno.h>

#include <sys/types.h>

#define KEY_SHAREDMEMORY 8
#define KEY_SEMAPHORE 3

#define mtype_demandeConsultation 11
#define mtype_demandeReservation 21

#define RESERVATION_KEY 31
#define CONSULTATION_KEY 32
#define SERVEUR_KEY 34

#define MSG_SIZE 100
#define TAB_SIZE 100
#define NB_FUSEE 4

#define NB_PLACES_MIN 1
#define NB_PLACES_MAX 4

typedef enum {CONS = 1, RES = 2} typeRequest_t;

typedef struct
{
    int jour;
    int mois;
    int annee;
} date_t;

typedef struct
{
    date_t date;
    int fusee[NB_FUSEE];
} reservation;

typedef struct
{
    reservation reservation[TAB_SIZE];
    int taille;
} tabReservation;

typedef struct
{
    long mtype;
    pid_t client;
    date_t date;
    int nbPlace;
} reqReservation_t;

typedef struct
{
    long mtype;
    pid_t client;
    date_t date;
} reqConsultation_t;

typedef struct
{
    long mtype;
    int nbplace;
} ansConsultation_t;

typedef struct
{
    long mtype;
    char message[MSG_SIZE];
} ansReservation_t;

//« sans » réservation
reservation resa_default = {{0, 0, 0}, {6, 12, 12, 6}};

//message queue
static int msqidReservation;
static int msqidConsultation;
//mémoire partagée
static int shmid;
//sémaphore
static int semid;

#endif // SERVEUR_H_INCLUDED
