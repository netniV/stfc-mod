# Manual Ship Identification Guide

Based on your extracted data, you have 38 ships. Let's identify them manually using:
1. Hull IDs from your gamestate
2. Tier information
3. Your knowledge of which ships you own
4. Ship blueprint resources

## Your Ships by Tier

### Tier 1 Ships (12 ships)
These are starter/early game ships:

| Hull ID | Common Tier 1 Ships |
|---------|---------------------|
| 3046584086 | Appears 5 times - likely **Turas** (most common T1) |
| 1842444641 | Appears 5 times - likely **Jellyfish** or **Realta** |
| 393740394 | Appears 3 times - likely **ECS Fortunate** or **Phindra** |
| Others | **Talla**, **Orion Corvette**, **Vahklas**, etc. |

### Tier 3 Ships (2 ships)
| Hull ID | Possible Ships |
|---------|----------------|
| ? | **Kumari**, **D3**, **Kehra** |

### Tier 4 Ships (10 ships)
Common mid-game ships:
| Hull ID | Possible Ships |
|---------|----------------|
| 987222969 | **Defiant** (you have Defiant_Tier_3_Token resource!) |
| Others | **Enterprise**, **Augur**, **Saladin**, **Centurion**, **Legionary** |

### Tier 5 Ships (1 ship)
| Hull ID | Possible Ships |
|---------|----------------|
| ? | **Bortas**, **Pilum**, **Gladius** |

### Tier 6 Ships (2 ships)
| Hull ID | Possible Ships |
|---------|----------------|
| ? | **Vidar**, **Talios**, **Vahklas** (advanced) |

### Tier 7 Ships (3 ships)
| Hull ID | Possible Ships |
|---------|----------------|
| 2919480363 | Level 35 - could be **Franklin-A** or **Stella** |
| Others | **Amalgam**, **Cerritos** |

### Tier 8 Ships (2 ships)
| Hull ID | Possible Ships |
|---------|----------------|
| ? | **Titan-A**, **D4** |

### Tier 9 Ships (6 ships)
High-end ships:
| Hull ID | Possible Ships |
|---------|----------------|
| 1029262994 | Level 45 |
| 1087128295 | Level 45 |
| Others | **Voyager**, **Defiant** (T9), **Enterprise** (T9) |

## How to Identify Your Ships

1. **Check your ship roster in-game** - Note which ships you have at each tier
2. **Match hull_ids** - Ships with the same hull_id appearing multiple times are common/survey ships
3. **Use blueprint resources** - You have:
   - `Resource_Defiant_Tier_3_Token` - confirms you have/had Defiant
   - `Resource_Parts_Battleship_G2/G3/G4` - G-series battleships
   - `Resource_BorgCube_Ship_Parts` - Borg Cube
   - `Resource_Mudd_Ship_R3` - Mudd's ship

4. **Check stfc.space manually**:
   - Go to https://stfc.space/ships/987222969 (change the ID)
   - Note the ship name from the page title or content
   - Build the mapping

## Next Step: Create Partial Mappings

Once you identify a few ships, we can add them to `stfc_id_mappings.json`:

```json
{
  "ships": {
    "987222969": {
      "name": "Defiant",
      "type": "Interceptor",
      "faction": "Federation"
    },
    "2919480363": {
      "name": "Franklin-A",
      "type": "Explorer", 
      "faction": "Federation"
    }
  }
}
```

Then test the enriched exports!
