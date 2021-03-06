/*
 *  image.cpp
 *  climso-auto
 *
 *  Created by Maël Valais on 15/04/2014.
 *
 *
 * J'ai écrit cette classe pour simplifier l'utilisation des nombreuses fonctions sur des images, du type
 * uneFonction(double** tab_entree, tab_entree_larg, tab_entree_haut, double** tab_sortie, int tab_sortie_larg, int tab_sortie_long)
 *
 * Une image équivaut à un tableau 2D de "double" (pour rester compatible avec les fonctions existantes)
 * J'ai choisit la convention "matrice" : un point est désigné par point(ligne,colonne)
 * en partant du point extrème nord-ouest. D'autres représentations conseillent point(x,y)
 * mais je n'ai pas pris cette convention.
 *
 * NOTE1: Concernant la représentation en mémoire de l'image, j'hésite encore beaucoup entre la représentation
 * linéaire (un seul tableau de "double" avec les lignes les unes à la suite des autres) et la représentation
 * matricielle, c'est à dire un tableau de tableaux de "double".
 * 			-> j'ai choisi une représentation linéaire, mais non-compatible avec les fonctions antérieures
 * 			mais par contre compatibles avec les fonctions d'affichage Qt par exemple
 * NOTE2: Après pas mal de recul, je pense que cette représentation linéaire n'aide pas lors des optimisations
 * car pour le calcul (type correlation), on manipule des pointeurs pour optimiser... Or, ça veut dire qu'on
 * utilise plus les getters et donc la classe telle qu'elle est définie est moins "solide".
 *
 * LES INSTANCES NE RESTENT PAS "en place":
 * Cette classe a un gros soucis avec la création multiple d'objets lourds : à chaque fois qu'on traite
 * une image, on crée un nouvel objet en mémoire. Dans une boucle, cela ralenti le processus...
 *
 * QUEL TYPE POUR STOCKER CHAQUE PIXEL:
 * Pourquoi utiliser des doubles alors que de simples 16-bits (ushort par exemple) sont suffisants
 * et codent pour 65535 tons ? En fait, on a besoin de ces doubles uniquement dans le cas de la correlation.
 * Du coup, on pourrait juste utiliser un tableau de double pour la correlation et ensuite copier dans l'image
 * résultat...
 *
 * VITESSE ENTRE LES FONCTIONS DE CORRELATION:
 * J'ai calculé les temps de calcul entre les fonctions correlation_rapide et correlation_lk. Au tout début, j'ai
 * cru que les performances de mon algorithme étaient moins bonnes, mais il s'est avéré que le nombre de points
 * pris en compte sur la référence étaient différents à cause du mode de calcul du seuil_reference.
 * Les vitesses sont donc les mêmes à 1 ou 2 ms près. Donc je choisirai la fonction recodée correlation_rapide
 * car elle ne requiert pas les fichiers convol.c et fcts_LK3.c.
 *
 *	VOCABULAIRE TIFF :
 *	- Strip : un ensemble de lignes ; généralement nb(strips)=nb(lignes)
 *	- Pixel : ensemble de Samples ; en nuances de gris, nb(Samples)=1
 *	- Sample : sous-partie du pixel ; 3 par pixel pour une image en couleurs, 1 pour du gris
 *
 * Problème avec la classe Image : l'utilisation d'objets avec allocation dynamique a deux pbms :
 * 		- les allocation dynamiques à chaque fois (environ 10 à 40 mo par objet) ralentissent
 */

#include <cmath>
#include "image.h"

/**
 * Crée une Image sans taille
 */
Image::Image() {
    lignes = 0;
    colonnes = 0;
    img = NULL;
    max_c = max_l = min_c = min_l = -1;
}

/**
 * Crée une image vierge selon les grandeurs données ; les pixels de l'images ne sont pas initialisées
 * @param hauteur
 * @param largeur
 */
Image::Image(int hauteur, int largeur) {
    lignes = hauteur;
    colonnes = largeur;
    img = new MonDouble[lignes*colonnes];
    max_c = max_l = min_c = min_l = -1;
}

/**
 * Constructeur par recopie
 * @param src L'image à copier
 */
Image::Image(Image& src) {
	lignes = src.lignes;
    colonnes = src.colonnes;
    img = new MonDouble[lignes*colonnes];
    max_c = max_l = min_c = min_l = -1;

    copier(src);
}

/**
 * Constructeur de recopie à partir d'une partie de l'image src
 * @param src
 * @param ligne_0 Coordonnées du point de départ de la partie copiée de src (nord-ouest du rectangle)
 * @param col_0
 * @param hauteur Grandeur du rectangle de copie
 * @param largeur
 */
Image::Image(Image& src, int ligne_0, int col_0, int hauteur, int largeur) {
    lignes = hauteur;
    colonnes = largeur;
    img = new MonDouble[lignes*colonnes];
    this->copier(src,ligne_0,col_0,hauteur,largeur);
    max_c = max_l = min_c = min_l = -1;
}

/**
 * Destructeur de la classe Image
 */
Image::~Image() {
    if(img != NULL)
        delete [] img;
}

/**
    Charge une image TIFF dans un objet Image
    @param fichierEntree Le fichier à charger
    @return NULL si erreur, la référence de Image sinon
	@author Nehad Hirmiz (http://stackoverflow.com/a/20170682)
		modifié par Mael Valais
	@note L'avertissement "TIFFReadDirectory: Warning, Unknown field with tag 50838 (0xc696) encountered"
			veut dire que quelques tags du fichier TIFF n'ont pas pu être interprétés, ce qui veut
			certainement dire que le fichier contient des metadata personnalisés non reconnus.
			L'erreur n'a aucune influence sur le traitement.
	@exception FormatPictureException L'image n'est pas en échelles de gris sur 16 ou 8 bits
	@exception OpeningPictureException L'image ne peut être lue
*/
Image* Image::depuisTiff(string fichierEntree) {
    TIFF* tif = TIFFOpen(fichierEntree.c_str(), "r");
    if (tif == NULL) {
		throw OpeningException(fichierEntree);
	}
	uint32_t imagelength,imagewidth;
	tdata_t buffer;
	uint32_t ligne;
	uint16_t samplePerPixel, bitsPerSample;
	
	TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &imagelength);
	TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &imagewidth);
	TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bitsPerSample); // sample = partie d'un pixel
	TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &samplePerPixel);
	

	if(bitsPerSample > 16) // Vérification qu'on est bien en 16 bits max
		throw FormatException(bitsPerSample,samplePerPixel,fichierEntree);
	if(samplePerPixel == 3 || samplePerPixel == 4) // FIXME: si samplePerPixel=0, ça signifie quoi ?
		throw FormatException(bitsPerSample,samplePerPixel,fichierEntree);

	Image *out = new Image(imagelength,imagewidth);
	
	buffer = _TIFFmalloc(TIFFScanlineSize(tif));
			
	for (ligne = 0; ligne < imagelength; ligne++)
	{
		TIFFReadScanline(tif, buffer, ligne, 0);
		for(int col=0; col < imagewidth; col++) { // Copie de la ligne buf dans img[]
			if(bitsPerSample == 16) // XXX 16 -> 16bits va un peu baisser les intensités
				out->setPix(ligne, col, (MonDouble)((uint16_t*)buffer)[col]); // pourquoi avec double ça marche ??
			else if (bitsPerSample == 8) // OK
                out->setPix(ligne, col, (MonDouble)((uint8_t*)buffer)[col]);
		}
	}
	_TIFFfree(buffer);
	TIFFClose(tif);
	return out;
}

/**
 * @param fichierSortie
 * @author Nehad Hirmiz (http://stackoverflow.com/a/20170682)
 * 		modifié par Mael Valais
 */
void Image::versTiff(string fichierSortie) {
	TIFF* out = TIFFOpen(fichierSortie.c_str(), "w");
	if (out == NULL) {
		throw OpeningException(fichierSortie);
	}
	TIFFSetField(out, TIFFTAG_SUBFILETYPE,0); // Nécessaire pour etre lue par Apercu.app
	TIFFSetField(out, TIFFTAG_IMAGELENGTH, lignes);
	TIFFSetField(out, TIFFTAG_IMAGEWIDTH, colonnes);
	TIFFSetField(out, TIFFTAG_SAMPLESPERPIXEL, NOMBRE_SAMPLES_PAR_PIXEL);
	TIFFSetField(out, TIFFTAG_BITSPERSAMPLE, NOMBRE_BITS_PAR_SAMPLE);
	TIFFSetField(out, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT); // Orig de l'image
	//   Some other essential fields to set that you do not have to understand for now.
	TIFFSetField(out, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
	TIFFSetField(out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK); // Min Is Black
	TIFFSetField(out, TIFFTAG_IMAGEDESCRIPTION,"Image generee par Image.cpp");
	
	tsize_t linebytes = colonnes * NOMBRE_SAMPLES_PAR_PIXEL * NOMBRE_BITS_PAR_SAMPLE/8; // length in memory of one row of pixel in the image.
	uint16_t *buf_ligne = NULL;
	// Allocating memory to store the pixels of current row
	if (TIFFScanlineSize(out) == linebytes)
		buf_ligne =(uint16_t *)_TIFFmalloc(linebytes);
	else
		buf_ligne = (uint16_t *)_TIFFmalloc(TIFFScanlineSize(out));
	
	// We set the strip size of the file to be size of one row of pixels
	TIFFSetField(out, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(out, colonnes*NOMBRE_SAMPLES_PAR_PIXEL));
	
	// On copie chaque ligne de l'image dans le fichier TIFF
	for (int l = 0; l < lignes; l++) {
		// On copie la ligne dans le buffer
		for (int c = 0; c < colonnes; c++) {
			buf_ligne[c] = (uint16_t)getPix(l,c);
		}

		if (TIFFWriteScanline(out, buf_ligne, l, 0) < 0)
			break;
	}
	(void) TIFFClose(out);
	if (buf_ligne)
		_TIFFfree(buf_ligne);
}

#if INCLUDE_SBIGIMG
/**
 * Génère un objet de la classe Image à partir d'un objet de la classe CSBIGImg ;
 * CSBIGImg (déclaré dans csbigimg.h) est utilisé par la caméra (csbigcam.h)
 * @param img
 * @return
 */
Image* Image::depuisSBIGImg(CSBIGImg &img) {
	Image* newImage = new Image(img.GetHeight(), img.GetWidth());
	for(int i=0; i < newImage->lignes; i++) {
		for(int j=0; j < newImage->colonnes; j++) {
			newImage->setPix(i,j,img.GetImagePointer()[i*newImage->colonnes + j]);
        }
    }
	return newImage;
}
#endif
/**
 * Génère une Image à partir d'un tableau à 2 dimensions de "double"
 * @param tableau
 * @param hauteur
 * @param largeur
 * @return
 */
Image* Image::depuisTableauDouble(double **tableau, int hauteur, int largeur) {
    Image* img_out = new Image(hauteur,largeur);
	for (int lign=0; lign < img_out->lignes; lign++) {
		for (int col=0; col < img_out->colonnes; col++) {
            img_out->setPix(lign, col, tableau[lign][col]);
		}
	}
    return img_out;
}

/**
 * @return Tableau à deux dimensions tableau[lignes][colonnes]
 * Doit être supprimé avec des for(l) delete [] tab[l]; delete [] tab;
 */
double** Image::versTableauDeDouble() {
	double** tab = new double*[lignes];
	for (int lign=0; lign< lignes; lign++) {
		tab[lign] = new double[colonnes];
		for (int col=0; col< colonnes; col++) {
			tab[lign][col] = getPix(lign, col);
		}
	}
	return tab;
}
/**
 * Traduit l'image en tableau linéaire sans normalisation
 * @return Tableau de uchar : tableau[lignes * colonnes]
 * Doit être supprimé avec delete [] tab;
 */
unsigned char* Image::versUchar() {
	 double coef = 255./INTENSITE_MAX;
	unsigned char *tab = new unsigned char[lignes*colonnes];
	for (int lign=0; lign < lignes; lign++) {
	for (int col=0; col < colonnes; col++) {
	tab[lign*colonnes + col] = getPix(lign, col)*coef;
	}
	}
	return tab;
}

/**
 * Traduit l'image en tableau linéaire avec normalisation
 * @return Tableau de uchar : tableau[lignes * colonnes]
 * Doit être supprimé avec delete [] tab;
 */
unsigned char* Image::versUcharEtNormaliser() {
    // dst(l,c) = ((src(l,c) - min)* coef + 0)
	// coef = (255 - 0)/(max-min)
    MonDouble valMin = valeurMin(), valMax = valeurMax();
    double coef = (255. - 0.)/(valMax - valMin);
    unsigned char *tab = new unsigned char[lignes*colonnes];
    for (int lign=0; lign < lignes; lign++) {
        for (int col=0; col < colonnes; col++) {
            tab[lign*colonnes + col] = (getPix(lign, col) - valMin) * coef;
        }
    }
    return tab;
}

/**
 * Copie une image src dans l'image receveuse avec le décalage
 * à partir d'en haut à gauche. Cela permet de prendre une partie
 * de l'image src et la mettre dans l'image receuveuse
 * @param src
 * @param l_decal
 * @param c_decal
 * @param hauteur
 * @param largeur
 */
void Image::copier(Image& src, int l_decal, int c_decal, int hauteur, int largeur) {
    int haut_cpy = min(hauteur,src.lignes);
    int larg_cpy = min(largeur, src.colonnes);
    
	int l_deb = max(0,0+l_decal);
	int c_deb = max(0, 0+c_decal);
	int l_fin = min(src.lignes, haut_cpy+l_decal);
	int c_fin = min(src.colonnes, larg_cpy+c_decal);
    
	for (int l = l_deb; l < l_fin; ++l) {
		for (int c = c_deb; c < c_fin; ++c) {
			setPix(l-l_decal,c-c_decal,src.getPix(l,c));
		}
	}
}

/**
 * Copie intégralement l'image src dans l'image receuveuse (dans la limite de sa taille)
 * @param src L'image à copier
 */
void Image::copier(Image& src) {
	copier(src,0,0,src.lignes,src.colonnes);
}

/**
 * Initialise l'image à une valeur donnée
 * @param val
 */
void Image::init(int val) {
	for (int l = 0; l < lignes; ++l) {
		for (int c = 0; c < colonnes; ++c) {
			setPix(l,c,val);
		}
	}
}


/**
 * Correlation de l'image receveuse avec l'image référence,
 * avec normalisation de l'espace de correlation sur [0, INTENSITE_MAX]
 * @param reference L'image de référence
 * @param seuil_ref Le seuil minimal de prise en compte des valeurs des pixles de la référence, entre 0 et 1
 * @return L'espace de corrélation
 */
Image* Image::correlation_simple(Image& reference, float seuil_ref) {
	Image* obj = this;
	Image* ref = new Image(reference);

	ref->normaliser(); // normalisation pour le seuil
	MonDouble seuil_relatif = seuil_ref*INTENSITE_MAX;


	/*
	 * Pseudo-code 1 : la convolution par la définition standard (c(x,y) = intégrale f(s)g(x-s))
	 * 	Pour tous les décalages possibles entre obj (==g) et ref (==f),
	 * 		Pour tous les point de ref,
	 * 			Si le point décalé de ref dans obj existe
	 * 				On ajoute le produit du point d'obj décalé et celui de ref dans l'espace de convolution
	 * 			Fin si
	 * 		Fin boucle
	 * 	Fin boucle
	 *
	 * On a inversé les deux boucles pour utiliser la partimonie sur ref :
	 *
	 *	Pour tous les point de ref,
	 *		Si le point de ref est au dessus du seuil,
	 *			Pour tous les décalages possibles entre obj et ref,
	 * 				Si le point décalé de ref dans obj existe
	 * 					On ajoute le produit du point d'obj décalé et celui de ref dans l'espace de convolution
	 * 				Fin si
	 * 			Fin boucle
	 * 		Fin si
	 * 	Fin boucle
	 *
	 * Ici,
	 * 		(l_ref,c_ref) parcourt la référence,
	 * 		(l_decalage,c_decalage) represente les vecteurs décalage possibles entre obj(0,0) et ref(0,0)
	 * Ensuite, deux "alias" :
	 * 		(l_obj,c_obj) est le point dans obj correspondant au point dans ref avec le décalage,
	 * 		(l_convol,c_convol) est le point dans convol (qui est l'espace de convolution) correspondant
	 * 			à une valeur du décalage (l_decalage,c_decalage). Tous les vecteurs décalage ont une valeur
	 * 			dans l'image convol.
	 */
    Image *convol = new Image(obj->getLignes()+ref->getLignes()-1, obj->getColonnes()+ref->getColonnes()-1);
	convol->init(0);
    
	double temps_calcul = (double)(clock());


	for (int l_ref=0; l_ref < ref->lignes; l_ref++) {
		for (int c_ref=0; c_ref < ref->colonnes; c_ref++) {
			if(ref->getPix(l_ref,c_ref) > seuil_relatif) {
				for (int l_decalage=-ref->lignes; l_decalage < obj->lignes; l_decalage++) {
					for (int c_decalage=-ref->colonnes; c_decalage < obj->colonnes; c_decalage++) {
						int l_obj = l_ref+l_decalage, c_obj = c_ref+c_decalage;
						int l_convol = l_decalage+ref->lignes-1, c_convol = c_decalage + ref->colonnes-1;
                        
						if(l_obj >= 0 && l_obj < obj->lignes && c_obj >= 0 && c_obj < obj->colonnes) {
							convol->setPix(l_convol,c_convol,
									convol->getPix(l_convol,c_convol)
									+ ref->getPix(l_ref,c_ref)
									* obj->getPix(l_obj,c_obj));
						}
					}
				}
			}
		}
	}

	printf ("Temps calcul = %4.2f s \n",  (double)(clock() - temps_calcul) /CLOCKS_PER_SEC);

    delete ref;
	convol->normaliser();
	return convol;
}

/**
 * Correlation de l'image receveuse avec l'image référence,
 * avec normalisation de l'espace de correlation sur [0, INTENSITE_MAX]
 * et optimisation par utilisation de pointeurs au lieu des getters/setters
 * @param reference L'image de référence
 * @param seuil_ref Le seuil minimal de prise en compte des valeurs des pixles de la référence, entre 0 et 1
 * @return L'espace de corrélation
 */
Image* Image::correlation_rapide(Image& reference, float seuil_ref) {
	Image* obj = this;
	Image* ref = new Image(reference);
    
	ref->normaliser(); // normalisation pour le seuil
	MonDouble seuil_relatif = seuil_ref*INTENSITE_MAX;
    
    Image *convol = new Image(obj->getLignes()+ref->getLignes()-1, obj->getColonnes()+ref->getColonnes()-1);
	convol->init(0);
    
	int haut_convol = obj->lignes+ref->lignes-1;
	int larg_convol = obj->colonnes+ref->colonnes-1;
#if DEBUG
	double temps_calcul = (double)(clock());
#endif

    double nbboucles=0;

	for (int l_ref=0; l_ref < ref->lignes; l_ref++) {
		for (int c_ref=0; c_ref < ref->colonnes; c_ref++) {
            int ref_pix = ref->getPix(l_ref,c_ref);
			if(ref_pix > seuil_relatif) {
				// On calcule quels point de "convol" correspondent à des décalages
				// valides (c'est à dire provoquant une intersection entre "ref" et "obj") ;
				// Un décalage est un vecteur (l_decal, c_decal) équivalent à (l_convol,c_convol)
				// entre ref(nblignes-1,nbcolonnes-1) et obj(0,0)
				//
				//			l_obj = l_ref+l_decalage-(ref->lignes-1);       (1)
				//			c_obj = c_ref+c_decalage-(ref->colonnes-1);     (2)
                
				int l_decal_deb = max(0,        0-l_ref+(ref->lignes-1)); // (cf (1) inversée)
				int c_decal_deb = max(0,        0-c_ref+(ref->colonnes-1));
				int l_decal_fin = min(haut_convol, 	obj->lignes-l_ref+(ref->lignes-1));
				int c_decal_fin = min(larg_convol, 	obj->colonnes-c_ref+(ref->colonnes-1));
                int haut_decal = l_decal_fin - l_decal_deb;
                int larg_decal = c_decal_fin - c_decal_deb;
                
                // LES DIFFICULTÉS SONT DE TROUVER LES BONS POINTEURS INITIAUX
                MonDouble* convol_pt = convol->ptr() + l_decal_deb*convol->colonnes + c_decal_deb;
                int l_obj_pt_initial = l_ref+l_decal_deb-(ref->lignes-1);
                int c_obj_pt_initial = c_ref+c_decal_deb-(ref->colonnes-1);
                MonDouble* obj_pt = obj->ptr() + l_obj_pt_initial*obj->colonnes + c_obj_pt_initial;
                for (int l_decal=0; l_decal < haut_decal; l_decal++) {
                	for (int c_decal=0; c_decal < larg_decal; c_decal++) {
                		*convol_pt += ref_pix * (*obj_pt);
                		obj_pt += 1;
                		convol_pt += 1;
                		nbboucles++;
                	}
                	// PUIS D'AVANCER CES POINTEURS DE LA BONNE FAÇON QUAND ON PASSE A LA LIGNE SUIVANTE
                    // On passe à la ligne suivante sur convol et obj
                	//obj_pt += 1; // ATTENTION, le for avance de 1, donc pas besoin (en sortie de for) d'avancer de nouveau
                	convol_pt += convol->colonnes - larg_decal; // ATTENTION, pas de +1 non plus ici
                }
			}
		}
	}
#if DEBUG
	printf ("Temps calcul = %4.2f s (%.0f boucles)\n",  (double)(clock() - temps_calcul) /CLOCKS_PER_SEC, nbboucles);
#endif
	delete ref;
	convol->normaliser();
	return convol;
}

/**
 * Correl où l'espace de correl est limité à l'image "obj", donc on n'étudie pas
 * les cas de décalage où la référence n'est pas incluse dans l'objet
 * Utilise correlation_rapide
 * @param reference L'image de référence
 * @param seuil_ref Le seuil minimal de prise en compte des valeurs des pixles de la référence, entre 0 et 1
 * @return L'espace de corrélation
 */
Image* Image::correlation_rapide_centree(Image& reference, float seuil_ref) {
	Image* img = correlation_rapide(reference,seuil_ref);
	// FIXME: L'image "découpée" est environ 1 à 2 pixels en dessous de l'image qu'on devrait avoir (comparaison avec algo LK)
	Image* img_centree = new Image(*img,reference.lignes/2,reference.colonnes/2,img->lignes-(reference.lignes-1),img->colonnes - (reference.colonnes-1));
	img_centree->versTiff("t_obj_centre.tif");
	delete img;
	return img_centree;
}

#if INCLUDE_CONVOL
/**
 * Corrélation (de convol.c)
 * @author Laurent Koechlin (C) 2008
 * @param reference L'image de référence
 * @param seuil_ref Le seuil minimal de prise en compte des valeurs des pixles de la référence, entre 0 et 1
 * @return L'espace de corrélation
 */
Image* Image::correlation(Image& reference, float seuil_ref) {
	int min_l, min_c, max_l, max_c;
	reference.minMaxPosition(&min_l, &min_c, &max_l, &max_c);
	MonDouble seuil_relatif = reference.getPix(min_l, min_c)
			+ seuil_ref*(reference.getPix(max_l, max_c)-reference.getPix(min_l, min_c));

	double** obj = this->versTableauDeDouble();
	double** ref = reference.versTableauDeDouble();
	double** res = new double*[this->lignes];
	for(int l=0; l<this->lignes;l++)
		res[l] = new double[this->colonnes];

	calc_convol(obj,ref,res,colonnes,lignes,reference.colonnes,reference.lignes,seuil_relatif);

	Image* img_resultat = Image::depuisTableauDouble(res,lignes,colonnes);
	img_resultat->normaliser();

	for(int l=0; l<reference.lignes;l++)
		delete [] ref[l];
	delete [] ref;
	for(int l=0; l<this->lignes;l++)
		delete [] obj[l];
	delete [] obj;
	for(int l=0; l<this->lignes;l++)
		delete [] res[l];
	delete [] res;

	return img_resultat;
}
#endif
/**
 * Affiche sur la sortie standard l'image en terme d'intensité (pour débug)
 */
void Image::afficher() {
	cout << "Affichage de l'image " << lignes << "x" << colonnes << endl;
	for (int l = 0; l < lignes; ++l) {
		for (int c = 0; c < colonnes; ++c) {
			cout << (int)this->getPix(l,c) << " ";
		}
		cout << endl;
	}
}

/**
 * Normalise l'image receveuse à [0, INTENSITE_MAX]
 */
void Image::normaliser() {
    normaliser(0,INTENSITE_MAX);
}

/**
 * Normalise l'image receveuse à [minSortie, maxSortie]
 */
void Image::normaliser(MonDouble minSortie, MonDouble maxSortie) {
    MonDouble min = getPix(posMinLigne(), posMinColonne());
    MonDouble max = getPix(posMaxLigne(), posMaxColonne());
    for (int l=0; l < lignes; l++) {
        for (int c=0; c < colonnes; c++) {
            // dst(l,c) = ((src(l,c) - min)*(MAX_SORTIE - MIN_SORTIE)/(max-min) + MIN_SORTIE)
            setPix(l, c,(getPix(l, c)-min)*(maxSortie - minSortie)/(max - min) + minSortie);
        }
    }
}

/**
 * Détermine les min et max en interne, si valMax, valMin,
 * posMax* ou posMin* demandé
 * @return Si la recherche a été effectuée ou non (dans le cas où une recherche minmax
 * a déjà été effectuée)
 */
bool Image::determinerMinMax() {
	// On vérifie si les min et max n'ont pas déjà été trouvés
	if(max_c!=-1 && max_l!=-1 && min_c!=-1 && min_l!=-1) {
		return false;
	}
    max_c = max_l = min_c = min_l = 0;
    for (int l=0; l < lignes; l++) {
        for (int c=0; c < colonnes; c++) {
            if(getPix(l,c) < getPix(min_l, min_c)) {
                min_l = l;
                min_c = c;
            }
            if(getPix(l,c) > getPix(max_l, max_c)) {
                max_l = l;
                max_c = c;
            }
        }
    }
    return true;
}

/**
 * Réduit d'un facteur donné la taille de l'image
 * @param binning La taille du carré de binning (binning 3x3 -> bin 3)
 * @return L'image réduite
 */
Image* Image::reduire(int binning) {
    Image *img_dst = new Image(lignes/binning, colonnes/binning);
    // On parcourt l'image de destination qui reçoit le bining (img_dst)
    for (int l_dst = 0 ; l_dst< img_dst->lignes ; l_dst++) {
        for (int c_dst = 0 ; c_dst< img_dst->colonnes ; c_dst++) {
            int l_src = l_dst * binning;
            int c_src = c_dst * binning;
            MonDouble somme = 0;
            // On parcourt le carré où on fait la moyenne du binning
            for (int l_tab_moy = l_src; l_tab_moy < l_src + binning; l_tab_moy++) {
                for (int c_tab_moy=c_src; c_tab_moy < c_src + binning; c_tab_moy++) {
                    somme += this->getPix(l_tab_moy, c_tab_moy);
                }
            }
            img_dst->setPix(l_dst, c_dst, somme / (binning*binning));
        }
    }
    return img_dst;
}

/**
 * Convolution de l'image par un noyau ; pas d'interpolation aux bords
 * @param noyau Matrice dans un tableau linéaire de taille taille*taille
 * @param taille Taille du noyau
 * @return Pointeur vers l'image convoluée
 * @note Ecrit
 */
Image* Image::convoluer(const int *noyau, int taille) {
    Image* img_dst = new Image(lignes,colonnes);
    // Parcourt de l'image à convoluer
    for (int l=0; l < lignes-(taille-1) ; l++) { // Bords exclus
		for (int c=0 ; c < colonnes-(taille-1) ; c++) {
            MonDouble somme = 0;
            // Parcourt du noyau de convolution
            for (int l_noyau=0; l_noyau < taille; l_noyau++) {
                for (int c_noyau=0; c_noyau < taille; c_noyau++) {
                    somme += getPix(l+l_noyau, c+c_noyau) * noyau[l_noyau*taille+c_noyau];
                }
            }
            img_dst->setPix(l+ taille/2, c + taille/2, (somme<0)?0:somme);
		}
    }
    return img_dst;
}

/**
 * Calcule la dérivée au carré de l'image ; on fait ça au lieu d'utiliser un noyau
 * de convolution Laplacien par exemple. Permet entre autres de diminuer la sensibilité
 * au bruit.
 * @return L'image résultat
 *
 * @author LK
 */
Image* Image::deriveeCarre() {
	// FIXME: utliser la fonction codée par LK
	return NULL;
}

/**
 * Draws a doughnut-shaped region that can be used for example as a spatial frequency filter ;
 * it will be drawn into the receiver object
 * @author Lk@2010, traduit d'un code LK 2010 en python
 * @author Mv@2014 pour le portage dans la classe Image
 *
 * @param c_centre Center column of doughnut in the array
 * @param l_centre Center row
 * @param freq_min Inner radius of doughnut. If freq_min = marge_int = 0, there is no central hole : a disc is drawn.
 * @param marge_int Width in pixels of the inner margin of doughnut (Hanning smooth)
 * @param freq_max Outer radius of doughnut
 * @param marge_ext Width in pixels of the outer margin of doughnut (Hanning smooth)
 */
void Image::tracerDonut(int l_centre, int c_centre, double freq_min, double marge_int, double freq_max, double marge_ext) {
	this->init(0); // On initialise à 0
    // Defines corona at "1" between two concentric circles and "0" elsewhere
    double ra = freq_min;
    double rc = freq_max;
    // Mask limits are smooth: go from 0 to 1 between ra and rb, then from 1 to 0 between rc and rd
    double rb = freq_min + marge_int;
    double rd = freq_max + marge_ext;

    double ra2 = ra * ra; 	// square these radii, for speed
    double rb2 = rb * rb;
    double rc2 = rc * rc;
    double rd2 = rd * rd;

    int c_min = (int) (c_centre - (freq_max + marge_ext));	// Shrink bounds, for speed
    int c_max = (int) (c_centre + (freq_max + marge_ext));
    int l_min = (int) (l_centre - (freq_max + marge_ext));
    int l_max = (int) (l_centre + (freq_max + marge_ext));

    if (c_min < 0) c_min=0; 	// Securities. Sould be shorter syntax such as 'cap((i_min,i_max,j_min,j_max), domainshape)'
    if (c_max < 0) c_max=0;
    if (l_min < 0) l_min=0;
    if (l_max < 0) l_max=0;
    if (c_min > this->colonnes) c_min = this->colonnes;
    if (c_max > this->colonnes) c_max = this->colonnes;
    if (l_min > this->lignes) l_min = this->lignes;
    if (l_max > this->lignes) l_max = this->lignes;

    for (int l = l_min ; l < l_max ; l++) {
        int dv = l - l_centre; 		// position center in v
        for (int c = c_min; c < c_max ; c++) {
            int dh = c - c_centre;       // position center in h
            int r2 = (dh*dh + dv*dv);	// square of current radius
            if (r2 < ra2)   this->setPix(l,c,0.); //image[v][h] = 0.;
            else if (r2 < rb2) {		// from 0 to 1, smoothly: 'Hanning window' like
                double dr = (sqrt(r2) - ra) / marge_int;
                this->setPix(l,c,(0.5*(1. - cos(PI * dr))));
            }
            else if (r2 < rc2)      setPix(l,c,1.);
            else if (r2 < rd2)		// from 1 to 0, smoothly
            {
                double dr = (sqrt(r2) - rc)  / (float)(marge_ext);
                setPix(l,c,0.5 * (1. + cos(PI * dr)));
            }
            else setPix(l,c,0.);
        }
    }
}
/**
 * @param diametre Le diamètre du soleil voulu
 * @return Une forme de soleil B/W dans une nouvelle image de taille appropriée
 */
Image* Image::tracerFormeSoleil(int diametre) {
	const double marge = 2.5;
	Image* img = new Image(diametre + 4*marge, diametre + 4*marge);
	img->tracerDonut(img->colonnes/2, img->lignes/2,0, 0, diametre/2 - marge/2, marge);
	img->normaliser();
	return img;
}

//----------------------- Fonctions LK ---------------------
#if INCLUDE_INTERPOL
/**
 * Interpolation d'une zone par Nevillle Aitken
 * Utilise la fonction d'interpolation d'un point codée par Lk@1999
 * @param l Coordonnées (ligne, colonne) du point autour duquel on interpole
 * @param c
 * @param pas_interp Finesse avec laquelle on interpole (1/8 par exemple)
 * @param taille Taille du carré d'interpolation
 * @return L'image du carré d'interpolation
 */
Image* Image::interpolerAutourDeCePoint(int l, int c, float pas_interp, float taille) {
	int marge_interp = taille/pas_interp;
	
	Image* interp = new Image(marge_interp,marge_interp);
	interp->init(0);
	
	// Création du tableau temporaire contenant l'objet receveur
	double** source = this->versTableauDeDouble();

	// Interpolation point par point en utilisant le tableau temporaire
	for (int l_interp = 0; l_interp < marge_interp; l_interp++) {
		for (int c_interp = 0; c_interp < marge_interp; c_interp++) {
			if(l_interp < this->getLignes() && c_interp < this->getColonnes()) {
				double l_correl = l_interp*pas_interp + l - taille/2;
				double c_correl = c_interp*pas_interp + c - taille/2;
				interp->setPix(l_interp, c_interp,
					it_pol_neville2D_s4(this->getLignes(), this->getColonnes(), source,
										l_correl, c_correl));
			}
		}
	}
	// Suppression du tableau temporaire
	for(int l=0; l<this->lignes; l++)
		delete [] source[l];
	delete [] source;
    return interp;
}
/**
 * Interpolation d'une zone par Nevillle Aitken
 * Utilise la fonction d'interpolation d'un point codée par Lk@1999
 * @param l Coordonnées (ligne, colonne) du point autour duquel on interpole
 * @param c
 * @return L'image du carré d'interpolation
 */
Image* Image::interpolerAutourDeCePoint(int l, int c) {
	const int marge = 20;
	const float pas_interp = 1/8;
	return interpolerAutourDeCePoint(l, c, pas_interp, marge);
}


/**
 * Trouve le maximum après interpolation de la zone maximale par Nevillle Aitken
 * @param l Coordonnées du point max trouvé
 * @param c
 */
void Image::maxParInterpolation(double *l, double *c) {
	const int taille = 20; // carré de 20 de pixels ; le max est au centre
	const float pas_interp = 1/8.0; // le pas d'interpolation
	Image *interp = this->interpolerAutourDeCePoint(posMaxLigne(), posMaxColonne(), pas_interp, taille);
	// On retrouve maintenant la position sub-pixel dans "this"
	*l = posMaxLigne() - taille/2.0 + interp->posMaxLigne() * pas_interp;
	*c = posMaxColonne() - taille/2.0 + interp->posMaxColonne() * pas_interp;
	delete interp;
}
#endif
/**
 * Fait une moyenne des alentours dans un carré de 100px de côté autour d'un point_donné,
 * en excluant les valeurs dans un carré de 50px de côté autour du point_donné,
 * puis donne le ratio moyenne_alentours/point_donné.
 * Le point C sera le pic ou le maximum présumé.
 * @param l Coordonnée ligne du point_donné
 * @param c Coordonnée colonnedu point_donné
 * @return ratio moyenne_alentours/point_donné ou -1 si aucune valeur possible
 */
double Image::calculerSignalSurBruit(int l_point, int c_point) {
	const int taille_carre_externe = 100;
	const int taille_carre_interne = 50;

	// On trouve les bornes min et max
	int l_deb = max(0, l_point - taille_carre_externe/2);
	int c_deb = max(0,c_point - taille_carre_externe/2);
	int l_fin = min(this->lignes, l_point + taille_carre_externe/2);
	int c_fin = min(this->colonnes, c_point + taille_carre_externe/2);

	double somme = 0;
	int compteur = 0;

	for(int l=l_deb; l<l_fin; l++) {
		for(int c=c_deb; c<c_fin; c++) {
			if(not((l > l_point-taille_carre_interne/2)
				&& (l < l_point + taille_carre_interne/2)
				&& (c > c_point-taille_carre_interne/2)
				&& (c < c_point + taille_carre_interne/2))) {
				somme += getPix(l,c); compteur++;
			}
		}
	}
	double point = getPix(l_point,c_point);
	if(compteur > 0) {
		double moyenne = somme/compteur;
		if(moyenne > 0) {
			return point/moyenne; // Calcul du signal/bruit
		}
		else return -1;
	}
	else return -1;
}


	/**
	 * Calcul la somme de : (valeur absolue de dérivée partielle en x + val.abs.deriv.partielle en y)
	 * d'un tableau 2D scalaire : |df/dx| + |df/dy|
	 * @author Lk@2014
	 * @param data
	 * @param absderiv
	 * @param size_x
	 * @param size_y
	 */
Image* Image::convoluerParDerivee() {
	Image* img = new Image(*this);
	img->init(0);
	double calcul;
	for (int l=1 ; l < lignes-1 ; l++) {
		for (int c=1 ; c < colonnes-1 ; c++) {
			calcul = sqrt((getPix(l,c)-getPix(l-1,c))*(getPix(l,c) - getPix(l-1,c))
					+ (getPix(l,c)-getPix(l,c-1))* (getPix(l,c)-getPix(l,c-1)));
			img->setPix(l,c,calcul);
		}
	}
	return img;
}

MonDouble Image::valeurMin() {
	if(max_c==-1 || max_l==-1 || min_c==-1 || min_l==-1) {
		determinerMinMax();
	}
	return getPix(min_l,min_c);
}

MonDouble Image::valeurMax() {
	if(max_c==-1 || max_l==-1 || min_c==-1 || min_l==-1) {
		determinerMinMax();
	}
	return getPix(max_l,max_c);
}

int Image::posMinLigne() {
	if(max_c==-1 || max_l==-1 || min_c==-1 || min_l==-1) {
		determinerMinMax();
	}
	return min_l;
}

int Image::posMinColonne() {
	if(max_c==-1 || max_l==-1 || min_c==-1 || min_l==-1) {
		determinerMinMax();
	}
	return min_c;
}

int Image::posMaxLigne() {
	if(max_c==-1 || max_l==-1 || min_c==-1 || min_l==-1) {
		determinerMinMax();
	}
	return max_l;
}

int Image::posMaxColonne() {
	if(max_c==-1 || max_l==-1 || min_c==-1 || min_l==-1) {
		determinerMinMax();
	}
	return max_c;
}
