/*
 * guidage.h
 *
 *  Created on: 16 juin 2014
 *      Author: Maël Valais
 *
 */

#ifndef GUIDAGE_H_
#define GUIDAGE_H_

#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtCore/QThread>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <QtCore/QTime>
#include <QtCore/QMetaType>
#include <QtCore/QTextStream>
#include <QtGui/QImage>
#include <QtGui/QColor>
#include "arduino.h"

/* 	Pour les paramètres: certains paramètres sont à configurer dans le fichier
 * 		~/.config/irap/climso-auto.conf (ou directement depuis menu > paramètres)
 * 	Paramètres par défaut: concernant ces réglages peuvent se trouver dans
 * 		la fonction chargerParametres()
 */

// Paramètres "en dur" ne pouvant être modifiés que par la recompilation :
#define PIN_NORD			12 // Numéro du pin sur lequel seront envoyées les commandes Nord
#define	PIN_SUD				11 // Numéro du pin sur lequel seront envoyées les commandes Sud
#define	PIN_EST				9 // Numéro du pin sur lequel seront envoyées les commandes Est
#define PIN_OUEST			10 // Numéro du pin sur lequel seront envoyées les commandes Ouest
#define POSITIONS_PAR_GUIDAGE	2 	// Nombre de positions (donc de captures) nécessaires avant de guider
#define PERIODE_ENTRE_CONNEXIONS	1000 // Période entre deux vérifications de connexion à l'arduino
#define DUREE_IMPULSION_MAX		10000 // en ms
#define INCREMENT_LENT		0.1 // Vitesse de déplacement de la consigne à chaque déplacement en nombre de pixels
#define INCREMENT_RAPIDE	1	// Vitesse de déplacement de la consigne à chaque déplacement en nombre de pixels
//#define SEUIL_DECALAGE_PIXELS		0.05 // Distance en pixels en dessous laquelle la position est estimée comme correcte


typedef enum {
	POSITION_COHERANTE, // La position courante est fiable vis à vis du bruit/signal
	POSITION_INCOHERANTE, // La position n'est pas fiable
	POSITION_NON_INITIALISEE // La position n'a pas été initialiasée auparavant
} EtatPosition;
typedef enum {
	CONSIGNE_OK, // La consigne et la position sont superposés selon le seuil
	CONSIGNE_LOIN, // La consigne et la position sont éloignés (envoi de commandes)
	CONSIGNE_DIVERGE, // Éloignement non contrôlé entre consigne et position -> vérifier branchements
	CONSIGNE_NON_INITIALISEE // La consigne n'a pas été initialiasée auparavant
} EtatConsigne;
typedef enum {
	ARDUINO_CONNEXION_ON, // L'arduino est connecté
	ARDUINO_CONNEXION_OFF, // L'arduino est déconnecté
	ARDUINO_FICHIER_INTROUV // Impossible d'ouvrir/de lire le fichier Arduino
} EtatArduino;
typedef enum {
	GUIDAGE_MARCHE, // Le guidage est en marche
	GUIDAGE_MARCHE_MAIS_BRUIT, // Le guidage marche toujours mais en attente d'un retour à un signal/bruit plus faible
	GUIDAGE_BESOIN_POSITION, // Le guidage a besoin d'une position courante du soleil
	GUIDAGE_ARRET_BRUIT, // Le guidage s'est arrêté pour cause de non-fiabilité (bruit/signal élevé)
	GUIDAGE_ARRET_NORMAL, // Le guidage s'est arrêté à la demande de l'utilisateur
	GUIDAGE_ARRET_DIVERGE, // Le guidage s'est arrêté à cause de l'éloignement non contrôlé consigne/position
	GUIDAGE_ARRET_PANNE // Le guidage s'est arrêté car l'arduino ou la caméra fait défaut
} EtatGuidage;

Q_DECLARE_METATYPE(EtatGuidage);
Q_DECLARE_METATYPE(EtatArduino);
Q_DECLARE_METATYPE(EtatPosition);
Q_DECLARE_METATYPE(EtatConsigne);

class Guidage: public QObject {
	Q_OBJECT
private:
	QTimer timerConnexionAuto;
	Arduino* arduino;

	// Résultats des captures envoyées par Capture
	double consigne_l, consigne_c;
	QImage img;	// Image envoyée par la classe Capture par le signal resultat()
	QList<double> position_l, position_c; // Historique des positions
	double signalbruit;
	int diametre; // Diametre du soleil en pixels pour l'affichage lorsqu'on utilisera repereSoleil(...)

	// Variables pour le guidage
	QTime tempsDernierePositionCoherente;
	QTime tempsDepuisDernierGuidage;
	QList<double> decalage; // Vecteur décalage entre la consigne et la position
	QList<QTime> decalageTimestamp;


	// Paramètres de guidage
	EtatArduino etatArduino;
	EtatConsigne etatConsigne;
	EtatPosition etatPosition;
	EtatGuidage etatGuidage;
	bool orientHorizInversee; // Paramètre
	bool orientVertiInversee; // Paramètre
	bool arretSiEloignement; // Paramètre
	int gainHorizontal; // Paramètre
	int gainVertical; // Paramètre
	double seuilSignalSurBruit; // Paramètre
	int dureeApresMauvaisSignalBruit; // Paramètre de durée en minutes

	// Autres paramètres
	QFile* fichier_log; // Fichier de log pour noter les positions et consignes
	QTextStream* logPositions; // Fichier stream lié à fichier_log pour noter positions et consignes
	bool etatAffichageRepereCourant; // Paramètre pour afficher le repère (cercle et croix centrale)
	bool etatAffichageRepereConsigne; // Paramètre pour afficher le repère (cercle et croix centrale)


	bool arduinoConnecte();
	void afficherImageSoleilEtReperes();
public:
	Guidage();
	~Guidage();
	static QStringList chercherFichiersArduino();
public slots:
	// guidage
	void lancerGuidage();
	void stopperGuidage();
	void modifierConsigne(int deltaLigne, int deltaColonne, bool decalageLent);
	void traiterResultatsCapture(QImage img, double l, double c, int diametre,double bruitsignal);
	// arduino
	void connecterArduino(QString nom);
	void deconnecterArduino();
	void envoyerCmd(int pin, int duree);
	void consigneReset();
	void enregistrerParametres();
	void chargerParametres();
	void afficherRepereConsigne(bool); // Permet d'afficher ou masquer les repères des positions
	void afficherRepereCourant(bool); // Permet d'afficher ou masquer les repères des positions
private slots:
	void guider();
	void connexionParTimer();
signals:
	void message(QString msg);
	void envoiEtatArduino(EtatArduino);
	void envoiEtatGuidage(EtatGuidage);
	void envoiEtatPosition(EtatPosition);
	void envoiEtatConsigne(EtatConsigne);
	void envoiPositionCourante(double x, double y);
	void envoiPositionConsigne(double x, double y);
	void imageSoleil(QImage);
	void repereCourant(float pourcent_x, float pourcent_y,float diametre_pourcent_x, EtatPosition);
	void repereConsigne(float pourcent_x, float pourcent_y,float diametre_pourcent_x, EtatConsigne);
	void signalBruit(double);
};

#endif /* GUIDAGE_H_ */
