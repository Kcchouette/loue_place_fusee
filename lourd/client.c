#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <time.h>

#include "serveur.h"

void startingMessage();

int clean_stdin();

int capChar(char* message);

date_t saisiDate(int typeRequest);

int saisiNbPlace();

int IsDateValid(int nDay, int nMonth, int nYear);

int IsDateValidAndLater(int nDay, int nMonth, int nYear);

int main()
{
    int request;
    reqReservation_t reservation;
    reqConsultation_t consultation;
    ansReservation_t ansRes;
    ansConsultation_t ansCons;

    int msqid;

    pid_t monPid = getpid();
    startingMessage();

    while(1)
    {
        printf("Veuillez sélectionner le type de requête que vous souhaitez effectuer :\n");
        printf("     1 pour une consultation, 2 pour une réservation ou un autre chiffre pour quitter\n");
        scanf("%d", &request);

        switch(request)
        {
        case CONS:
            consultation.client = monPid;
            consultation.date = saisiDate(CONS);
            consultation.mtype = mtype_demandeConsultation;

            if ((msqid = msgget(CONSULTATION_KEY, 0)) < 0)
            {
                perror("ERROR msgget consultation");
                exit(EXIT_FAILURE);
            }
            if (msgsnd(msqid, &consultation, sizeof(reqConsultation_t) - sizeof(long), 0) < 0)
            {
                perror("ERROR msgsnd consultation");
                exit(EXIT_FAILURE);
            }
            if (msgrcv(msqid, &ansCons, sizeof(ansConsultation_t) - sizeof(long), monPid, 0) == -1)
            {
                perror("ERROR msgrcv consultation");
                exit(EXIT_FAILURE);
            }


            printf("Il reste/restait %d places pour le %04d-%02d-%02d\n", ansCons.nbplace, consultation.date.annee, consultation.date.mois, consultation.date.jour);

            break;
        case RES:
            reservation.client = monPid;
            reservation.date = saisiDate(RES);
            reservation.nbPlace = saisiNbPlace();
            reservation.mtype = mtype_demandeReservation;

            if ((msqid = msgget(RESERVATION_KEY, 0)) < 0)
            {
                perror("ERROR msgget réservation");
                exit(EXIT_FAILURE);
            }

            if (msgsnd(msqid, &reservation, sizeof(reqReservation_t) - sizeof(long), 0) < 0)
            {
                perror("ERROR msgsnd réservation");
                exit(EXIT_FAILURE);
            }
            if (msgrcv(msqid, &ansRes, sizeof(ansReservation_t) - sizeof(long), monPid, 0) < 0)
            {
                perror("ERROR msgrcv réservation");
                exit(EXIT_FAILURE);
            }

            printf("%s\n", ansRes.message);

            break;
        default:
            printf("Vous avez choisi de quitter\n");
            printf("Exit\n");
            exit(EXIT_SUCCESS);
            break;
        }
    }
    return(EXIT_SUCCESS);
}


void startingMessage()
{
    printf("                                                                                                                    ___           \n");
    printf("                                                                                                                   /  /           \n");
    printf("                                                                                                                  /__/            \n");
    printf(" __       _______  __    __  _______   _______  __       _______  _______  _______   _______  __    __  _______  _______  _______ \n");
    printf("|  |     |  ___  ||  |  |  ||  _____| |  __   ||  |     |   _   ||  _____||  _____| |  _____||  |  |  ||  _____||  _____||  _____|\n");
    printf("|  |     | |   | ||  |  |  || |_____  | |__|  ||  |     |  |_|  || |      | |_____  | |_____ |  |  |  || |_____ | |_____ | |_____ \n");
    printf("|  |     | |   | ||  |  |  ||  _____| |   ____||  |     |   _   || |      |  _____| |  _____||  |  |  ||_____  ||  _____||  _____|\n");
    printf("|  |____ | |___| ||  |__|  || |_____  |  |     |  |____ |  | |  || |_____ | |_____  | |      |  |__|  | _____| || |_____ | |_____ \n");
    printf("|_______||_______||________||_______| |__|     |_______||__| |__||_______||_______| |_|      |________||_______||_______||_______|\n");
    printf("\n");
    printf(" _____________________________________________________________ \n");
    printf("|                                                             |\n");
    printf("| Bienvenue sur notre espace de réservation de place de fusée |\n");
    printf("|_____________________________________________________________|\n");
    printf("\n");
    printf("Avertissement : il y a un maximum de %d jours de réservation\n", TAB_SIZE);
    printf("\n");
    for(int i = 0; i < NB_FUSEE; ++i)
    {
        printf(" * %2d places réservables dans la fusée %2d\n", resa_default.fusee[i], i + 1);
    }
    printf("\n");
}

int clean_stdin()
{
    while (getchar()!='\n');
    return 1;
}

int capChar(char* message)
{
    int date;
    char c;
    do
    {
        printf("Veuillez saisir %s : ", message);
    }
    while(((scanf("%d%c",&date, &c)!=2 || c!='\n') && clean_stdin()));
    return date;
}

date_t saisiDate(int typeRequest)
{
    date_t date;
    printf("Veuillez choisir une date à venir: \n");
    date.jour = capChar("un jour");
    date.mois = capChar("un mois");
    date.annee = capChar("une année");

    if ((typeRequest == CONS) && (IsDateValid(date.jour, date.mois, date.annee)))
    {
        return (date);
    }
    else if ((typeRequest == RES) && (IsDateValidAndLater(date.jour, date.mois, date.annee)))
    {
        return (date);
    }
    printf("La date n'est pas valide.\n");
    return saisiDate(typeRequest);
}

int saisiNbPlace()
{
    int n;
    char c;
    do
    {
        printf("Veuillez saisir le nombre de place que vous souhaitez réserver (entre %d et %d) : ", NB_PLACES_MIN, NB_PLACES_MAX);
    }
    while(((scanf("%d%c",&n, &c)!=2 || c!='\n') && clean_stdin()) || n < NB_PLACES_MIN || n > NB_PLACES_MAX);
    return n;
}

int IsDateValid(int nDay, int nMonth, int nYear )
{
    if (nDay < 0 || nMonth < 0 || nYear < 0)
    {
        return(0);
    }

    //Vérifie que la date existe
    struct tm tmDate;
    memset( &tmDate, 0, sizeof(struct tm) );
    tmDate.tm_mday = nDay;
    tmDate.tm_mon = (nMonth - 1);
    tmDate.tm_year = (nYear - 1900);

    //copie la date dans une structure permettant de la validée
    struct tm tmValidateDate;
    memcpy( &tmValidateDate, &tmDate, sizeof(struct tm) );

    time_t timeCalendar = mktime( &tmValidateDate );
    if( timeCalendar == (time_t) -1 )
        return(0);

    return ((tmDate.tm_mday == tmValidateDate.tm_mday) &&(tmDate.tm_mon == tmValidateDate.tm_mon) && (tmDate.tm_year == tmValidateDate.tm_year) );
}

int IsDateValidAndLater(int nDay, int nMonth, int nYear )
{
    if (nDay < 0 || nMonth < 0 || nYear < 0)
    {
        return(0);
    }

    //Vérifie que la date existe
    struct tm tmDate;
    memset( &tmDate, 0, sizeof(struct tm) );
    tmDate.tm_mday = nDay;
    tmDate.tm_mon = (nMonth - 1);
    tmDate.tm_year = (nYear - 1900);

    //copie la date dans une structure permettant de la validée
    struct tm tmValidateDate;
    memcpy( &tmValidateDate, &tmDate, sizeof(struct tm) );

    time_t timeCalendar = mktime( &tmValidateDate );
    if( timeCalendar == (time_t) -1 )
        return(0);

    if ((tmDate.tm_mday == tmValidateDate.tm_mday) &&(tmDate.tm_mon == tmValidateDate.tm_mon) && (tmDate.tm_year == tmValidateDate.tm_year) )
    {
        //date d'aujourd'hui
        time_t now_t;
        struct tm now;
        time(&now_t);
        now = *localtime(&now_t);
        now.tm_hour = 0;
        now.tm_min = 0;
        now.tm_sec = 0;

        return (difftime(mktime(&tmDate), mktime(&now)) >= 0);
    }
    else
    {
        return (0);
    }
}
