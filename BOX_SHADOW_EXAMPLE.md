# Box Shadow Feature

Cette fonctionnalité ajoute le support des ombres portées (box-shadow) aux notifications mako.

## Utilisation

Ajoutez ces options dans votre fichier de configuration `~/.config/mako/config` :

### Ombre basique
```
box-shadow-offset=5,5,5,5
box-shadow-blur=10
box-shadow-color=#00000080
```

### Ombre décalée vers le bas-droite
```
box-shadow-offset=0,8,8,0
box-shadow-blur=15
box-shadow-color=#00000060
```

### Ombre sans flou (sharp shadow)
```
box-shadow-offset=3,3,3,3
box-shadow-blur=0
box-shadow-color=#000000FF
```

### Ombre colorée
```
box-shadow-offset=0,5,5,0
box-shadow-blur=20
box-shadow-color=#FF000040
```

## Options

- **box-shadow-offset** : Décalage de l'ombre sur chaque côté (top,right,bottom,left)
  - Exemple: `0,5,5,0` = ombre décalée de 5px vers la droite et le bas
  
- **box-shadow-blur** : Rayon de flou en pixels (0 = ombre nette)
  
- **box-shadow-color** : Couleur de l'ombre au format #RRGGBBAA
  - Les 2 derniers caractères contrôlent la transparence (00=transparent, FF=opaque)

## Ligne de commande

Vous pouvez aussi utiliser ces options en ligne de commande :

```bash
mako --box-shadow-offset=0,5,5,0 --box-shadow-blur=15 --box-shadow-color=#00000080
```

## Note

Par défaut, le box-shadow est désactivé (blur=0, offset=0,0,0,0).
