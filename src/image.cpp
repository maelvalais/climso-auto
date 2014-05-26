//
//  image.cpp
//  climso-auto
//
//  Created by Maël Valais on 15/04/2014.
//  Copyright (c) 2014 Maël Valais. All rights reserved.
//
//	Vocabulaire TIFF :
//	- Strip : un ensemble de lignes ; généralement nb(strips)=nb(lignes)
//	- Pixel : ensemble de Samples ; en nuances de gris, nb(Samples)=1
//	- Sample : sous-partie du pixel ; 3 par pixel pour une image en couleurs, 1 pour du gris
//
//

#include "image.h"

Image::Image() {
    lignes = 0;
    colonnes = 0;
    img = NULL;
}
/*
Image::Image(int hauteur, int largeur) {
    lignes = hauteur;
    colonnes = largeur;
    img = new MonDouble[lignes*colonnes];
}
Image::Image(Image& src) {
	lignes = src.lignes;
    colonnes = src.colonnes;
    img = new MonDouble[lignes*colonnes];

    copier(src);
}

Image::~Image() {
    if(img != NULL)
    	delete img;
}
*/

Image::Image(int hauteur, int largeur) {
    lignes = hauteur;
    colonnes = largeur;
    img = new MonDouble*[lignes];
    for(int l = 0; l < lignes; l++) {
    	img[l] = new MonDouble[colonnes];
    }
}
Image::Image(Image& src) {
	lignes = src.lignes;
    colonnes = src.colonnes;
    img = new MonDouble*[lignes];
    for(int l = 0; l < lignes; l++) {
    	img[l] = new MonDouble[colonnes];
    }
    copier(src);
}

Image::~Image() {
	if(img != NULL) {
		for(int l=0; l<lignes; l++) {
			delete [] img[l];
		}
		delete [] img;
	}
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
 *
 * @param fichierSortie
 * @author Nehad Hirmiz (http://stackoverflow.com/a/20170682)
 * 		modifié par Mael Valais
 *
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
	for (int lign=0; lign< img_out->lignes; lign++) {
		for (int col=0; col< img_out->colonnes; col++) {
            img_out->setPix(lign, col, tableau[lign][col]);
		}
	}
    return img_out;
}

void Image::versTableauDeDouble(double **tab, int *hauteur_dst, int *largeur_dst) {
    *hauteur_dst = lignes;
	*largeur_dst = colonnes;
	
	tab = new double*[lignes];
	for (int lign=0; lign< *hauteur_dst; lign++) {
		tab[lign] = new double[*largeur_dst];
		for (int col=0; col< *largeur_dst; col++) {
			tab[lign][col] = getPix(lign, col);
		}
	}
}

/**
 * Copie une image src dans l'image receveuse avec le décalage
 * à partir d'en haut à gauche
 * @param src
 * @param l_decal
 * @param c_decal
 * @param hauteur
 * @param largeur
 */
void Image::copier(Image& src, int l_decal, int c_decal, int hauteur, int largeur) {
    int haut_cpy = min(hauteur,src.lignes);
    int larg_cpy = min(largeur, src.colonnes);
    
	int l_deb = max(0,0+l_decal), c_deb = max(0, 0+c_decal);
	int l_fin = min(this->lignes, haut_cpy+l_decal), c_fin = min(this->colonnes, larg_cpy+c_decal);
    
	for (int l = l_deb; l < l_fin; ++l) {
		for (int c = c_deb; c < c_fin; ++c) {
			setPix(l,c,src.getPix(l+l_decal,c+c_decal));
		}
	}
}
void Image::copier(Image& src) {
	copier(src,0,0,src.lignes,src.colonnes);
}

void Image::init(int val) {
	for (int l = 0; l < lignes; ++l) {
		for (int c = 0; c < colonnes; ++c) {
			setPix(l,c,0);
		}
	}
}


/**
 * Correlation de l'image receveuse avec l'image référence,
 * avec normalisation de l'espace de correlation sur [0, INTENSITE_MAX]
 * @param ref L'image de référence
 * @param seuil Le seuil entre 0 et 1
 * @return L'espace de corrélation
 */
Image* Image::correlation_MV(Image& reference, float seuil_ref) {
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



// Correl où l'espace de correl est limité à l'image "obj", donc on n'étudie pas
// les cas de décalage où la référence n'est pas incluse dans l'objet
Image* Image::correlation_reduite_MV(Image& reference, float seuil_ref) {
	Image* obj = this;
	Image* ref = new Image(reference);
    
	ref->normaliser(); // normalisation pour le seuil
	MonDouble seuil_relatif = seuil_ref*INTENSITE_MAX;

    Image *convol = new Image(obj->getLignes()+ref->getLignes()-1, obj->getColonnes()+ref->getColonnes()-1);
	convol->init(0);
    
	int haut_convol = obj->lignes+ref->lignes-1;
	int larg_convol = obj->colonnes+ref->colonnes-1;

	double temps_calcul = (double)(clock());

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
                
				for (int l_decal=l_decal_deb; l_decal < l_decal_fin; l_decal++) {
					for (int c_decal=c_decal_deb; c_decal < c_decal_fin; c_decal++) {
						convol->setPix(l_decal,c_decal,
								convol->getPix(l_decal,c_decal)
								+ ref_pix
								* obj->getPix(l_ref+l_decal-(ref->lignes-1),c_ref+c_decal-(ref->colonnes-1)));
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
/*
// Correl où l'espace de correl est limité à l'image "obj", donc on n'étudie pas
// les cas de décalage où la référence n'est pas incluse dans l'objet
// avec opti pointeurs
Image* Image::correlation_reduite2_MV(Image& reference, float seuil_ref) {
	Image* obj = this;
	Image* ref = new Image(reference);
    
	ref->normaliser(); // normalisation pour le seuil
	MonDouble seuil_relatif = seuil_ref*INTENSITE_MAX;
    
    Image *convol = new Image(obj->getLignes()+ref->getLignes()-1, obj->getColonnes()+ref->getColonnes()-1);
	convol->init(0);
    
	int haut_convol = obj->lignes+ref->lignes-1;
	int larg_convol = obj->colonnes+ref->colonnes-1;
    
	double temps_calcul = (double)(clock());
    
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
                
                MonDouble* convol_pt = convol->ptr() + l_decal_deb*convol->colonnes + c_decal_deb;
                MonDouble* obj_pt = obj->ptr() + (l_ref+l_decal_deb-(ref->lignes-1))*obj->colonnes + (c_ref+c_decal_deb-(ref->colonnes-1));
                for (int l_decal=0; l_decal < haut_decal; l_decal++) {
                	for (int c_decal=0; c_decal < larg_decal; c_decal++) {
                		*convol_pt += ref_pix * (*obj_pt);
                		obj_pt += 1;
                		convol_pt += 1;
                	}
                    // On passe à la ligne suivante sur convol et obj
                	convol_pt += convol->colonnes - larg_decal + 1;
                	obj_pt += 1;
                }
			}
		}
	}

	printf ("Temps calcul = %4.2f s \n",  (double)(clock() - temps_calcul) /CLOCKS_PER_SEC);

	delete ref;
	convol->normaliser();
	return convol;
}
*/


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
    int l_min, c_min, l_max, c_max;
    minMaxPixel(&l_min, &c_min, &l_max, &c_max);
    MonDouble min = getPix(l_min, c_min);
    MonDouble max = getPix(l_max, c_max);
    for (int l=0; l < lignes; l++) {
        for (int c=0; c < colonnes; c++) {
            // dst(l,c) = ((src(l,c) - min)*INTENSITE_MAX /(max-min)
            setPix(l, c,(getPix(l, c)-min)*INTENSITE_MAX/(max-min));
        }
    }
}

/**
 * Trouve les positions des min, max
 * @param l_min Ligne du min  (ou NULL)
 * @param c_min Colonne du min (ou NULL)
 * @param l_max Ligne du max (ou NULL)
 * @param c_max Colonne du max (ou NULL)
 */
void Image::minMaxPixel(int *l_min, int *c_min, int *l_max, int *c_max) {
    if(l_min && c_min)
    	*l_min = *c_min = 0;
	if(l_max && c_max)
		*l_max = *c_max = 0;

    for (int l=0; l < lignes; l++) {
        for (int c=0; c < colonnes; c++) {
            if(l_min && c_min && getPix(l,c) < getPix(*l_min, *c_min)) {
                *l_min = l; *c_min = c;
            }
            if(l_max && c_max && getPix(l,c) > getPix(*l_max, *c_max)) {
                *l_max = l; *c_max = c;
            }
        }
    }
}
void Image::maxPixel(int *l, int *c) {
	minMaxPixel(NULL,NULL,l,c);
}

/**
 * Réduit d'un facteur donné la taille de l'image
 * @param facteur_binning Le facteur de binning (binning 2x2 -> facteur 2)
 * @return L'image réduite
 */
Image* Image::reduire(int facteur_binning) {
    Image *img_dst = new Image(lignes/facteur_binning, colonnes/facteur_binning);
    // On parcourt l'image de destination qui reçoit le bining (img_dst)
    for (int l_dst = 0 ; l_dst< img_dst->lignes ; l_dst++) {
        for (int c_dst = 0 ; c_dst< img_dst->colonnes ; c_dst++) {
            int l_src = l_dst * facteur_binning;
            int c_src = c_dst * facteur_binning;
            MonDouble somme = 0;
            // On parcourt le carré où on fait la moyenne du binning
            for (int l_tab_moy = l_src; l_tab_moy < l_src + facteur_binning; l_tab_moy++) {
                for (int c_tab_moy=c_src; c_tab_moy < c_src + facteur_binning; c_tab_moy++) {
                    somme += this->getPix(l_tab_moy, c_tab_moy);
                }
            }
            img_dst->setPix(l_dst, c_dst, somme / (facteur_binning*facteur_binning));
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



//----------------------- Fonctions LK ---------------------

#if INCLUDE_CONVOL
Image* Image::correlation_lk(Image& reference, float seuil_ref) {
	Image* ref = &reference;
	int min_l, min_c, max_l, max_c;
	ref->minMaxPixel(&min_l, &min_c, &max_l, &max_c);
	MonDouble seuil_relatif = seuil_ref*(ref->getPix(max_l, max_c) - ref->getPix(min_l, min_c));
	
	Image* correl = new Image(this->lignes,this->colonnes);

	calc_convol(this->ptr(), ref->ptr(), correl->ptr(), this->colonnes, this->lignes, ref->colonnes, ref->lignes, seuil_relatif);
	correl->normaliser();
	return correl;
}
#endif

#if INCLUDE_FCTS_LK3
Image* Image::dessinerMasqueDeSoleil(int diametre) {
	const double marge = 2.5;
	Image* img = new Image(diametre + 4*marge, diametre + 4*marge);

	draw_doughnut (img->ptr(), img->colonnes, img->lignes,
				   img->colonnes/2, img->lignes/2,
				   0, 0, diametre/2 - marge/2, marge);
	img->normaliser();
	return img;
}
#endif

#if INCLUDE_INTERPOL
Image* Image::interpolerAutourDeCePoint(int l, int c, float pas_interp, float taille) {
	int marge_interp = taille/pas_interp;
	
	Image* interp = new Image(marge_interp,marge_interp);
	interp->init(0);
	
	for (int l_interp = 0; l_interp < marge_interp; l_interp++) {
		for (int c_interp = 0; c_interp < marge_interp; c_interp++) {
			if(l_interp < this->getLignes() && c_interp < this->getColonnes()) {
				double l_correl = l_interp*pas_interp + l - taille/2;
				double c_correl = c_interp*pas_interp + c - taille/2;
				interp->setPix(l_interp, c_interp,
					it_pol_neville2D_s4(this->getLignes(), this->getColonnes(), this->ptr(),
										l_correl, c_correl));
			}
		}
	}
    return interp;
}
Image* Image::interpolerAutourDeCePoint(int l, int c) {
	const int marge = 20;
	const float pas_interp = 1/8;
	return interpolerAutourDeCePoint(l, c, pas_interp, marge);
}

void Image::maxParInterpolation(double *l, double *c) {
	const int taille = 20; // carré de 20 de pixels ; le max est au centre
	const float pas_interp = 1/8.0; // le pas d'interpolation

	int l_max, c_max;
	this->maxPixel(&l_max, &c_max);

	Image *interp = this->interpolerAutourDeCePoint(l_max, c_max, pas_interp, taille);
	
	int l_max_interp, c_max_interp;
	interp->maxPixel(&l_max_interp, &c_max_interp);
	
	// On retrouve maintenant la position sub-pixel dans "this"
	*l = l_max - taille/2.0 + l_max_interp * pas_interp;
	*c = c_max - taille/2.0 + c_max_interp * pas_interp;
	delete interp;
}

#endif

