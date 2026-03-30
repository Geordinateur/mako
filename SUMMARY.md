# Box-Shadow Feature - Summary

## Fonctionnalité ajoutée
Support complet des ombres portées (box-shadow) pour les notifications mako avec syntaxe CSS-like.

## Fichiers modifiés

### Core implementation
- **include/config.h** : Structures pour box-shadow (offset, blur, color, quality)
- **include/types.h** : Fonction parse_box_shadow() et struct mako_box_shadow
- **config.c** : Parsing des options + valeurs par défaut
- **types.c** : Implémentation parse_box_shadow() avec syntaxe CSS
- **render.c** : Algorithme de rendu avec gradient gaussien

### Documentation
- **doc/mako.5.scd** : Documentation des nouvelles options
- **BOX_SHADOW_EXAMPLE.md** : Guide d'utilisation avec exemples

## Syntaxe

### Simple (CSS-like)
```
box-shadow=offset-x offset-y blur [color] [quality]
```

Exemples :
```
box-shadow=0 4 8 #00000033 100
box-shadow=0 6 12 #00000040 150
```

### Détaillée (options séparées)
```
box-shadow-offset=top,right,bottom,left
box-shadow-blur=pixels
box-shadow-color=#RRGGBBAA
box-shadow-quality=10-200
```

## Algorithme de rendu

- **Clipping inversé** : L'ombre n'apparaît pas sous la notification
- **Gradient gaussien** : Falloff exponentiel `exp(-3*t²)` pour effet naturel
- **Multi-couches** : 10-200 couches (défaut: 100) pour blur smooth
- **Alpha optimisé** : Division par `layers * 0.35` pour correspondre aux ombres CSS

## Configurations recommandées

### Subtil (CSS standard)
```
box-shadow=0 4 8 #00000033 100
border-radius=12
```

### Material Design
```
box-shadow=0 6 12 #00000040 100
border-radius=8
```

### Diffus et doux
```
box-shadow=0 4 20 #00000033 100
border-radius=10
```

## Valeurs par défaut
- offset: 0,0,0,0 (pas d'ombre)
- blur: 0
- color: #0000007F (noir 50%)
- quality: 100

## Performance
- Quality 60-80 : Rapide, bon rendu
- Quality 100 : Équilibré (recommandé)
- Quality 150-200 : Très smooth mais plus lent

## Notes techniques
- Cap automatique à 100 couches effectives pour naturalité
- Support border-radius (ombre suit les coins arrondis)
- Compatible avec toutes les autres options de style mako
