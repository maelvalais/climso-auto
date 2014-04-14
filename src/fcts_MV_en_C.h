//
//  fcts_MV.h
//  climso-auto
//
//  Created by Maël Valais on 14/04/2014.
//  Copyright (c) 2014 Maël Valais. All rights reserved.
//

#ifndef climso_auto_fcts_MV_h
#define climso_auto_fcts_MV_h

typedef struct _date {
    int jour;
    int mois;
    int annee;
} Date;

int estBissextile(Date d);
short numeroDuJourDansAnnee(Date d);
short differenceEntreJoursDuneAnnee(Date d1, Date d2);

#endif