/* This file is part of GNU Dico.
   Copyright (C) 2012-2024 Sergey Poznyakoff

   GNU Dico is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   GNU Dico is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GNU Dico.  If not, see <http://www.gnu.org/licenses/>. */

#include <config.h>
#include <stdlib.h>
#include <string.h>
#include "gcide.h"

struct gcide_entity {
    char *ent;
    char *text;
};

static struct gcide_entity gcide_entity[] = {
    { "Cced",   "Ç" },
    { "uum",    "ü" },
    { "eacute", "é" },
    { "acir",   "â" },
    { "aum",    "ä" },
    { "agrave", "à" },
    { "aring",  "å" },
    { "ccedil", "ç" },
    { "cced",   "ç" },
    { "ccar",   "č" },
    { "cacute", "ć" },
    { "ecir",   "ê" },
    { "eum",    "ë" },
    { "egrave", "è" },
    { "ium",    "ï" },
    { "icir",   "î" },
    { "igrave", "ì" },
    { "Aum",    "Ä" },
    { "Aring",  "Å" },
    { "Eacute", "É" },
    { "ae",     "æ" },
    { "AE",     "Æ" },
    { "ocir",   "ô" },
    { "oum",    "ö" },
    { "ograve", "ò" },
    { "oacute", "ó" },
    { "Oacute", "Ó" },
    { "ucir",   "û" },
    { "ugrave", "ù" },
    { "uacute", "ú" },
    { "yum",    "ÿ" },
    { "Oum",    "Ö" },
    { "Uum",    "Ü" },
    { "pound",  "£", },
    { "aacute", "á" },
    { "iacute", "í" },
    { "dele",   "₰" }, /* FIXME: <dele/ is used to represent the Deleatur
			  sign in the entry for "Dele". There is no
			  corresponding symbol in Unicode, so the German
			  pfennig sign is used instead.  See
			  https://www.unicode.org/L2/L2021/21230-delete-sign.pdf
		       */

    /* Vulgar fractions */
    { "frac00",        "0/0" }, /* FIXME */
    { "frac12", "½" },
    { "frac13", "⅓" },
    { "frac14", "¼" },
    { "frac15", "⅕" },
    { "frac16", "⅙" },
    { "frac17", "⅐" },
    { "frac18", "⅛" },
    { "frac19", "⅑" },
    { "frac23", "⅔" },
    { "frac25", "⅖" },
    { "frac32", "³⁄₂" },
    { "frac34", "¾" },
    { "frac35", "⅗" },
    { "frac36", "³⁄₆" },
    { "frac37", "³⁄₇" },
    { "frac38", "⅜" },
    { "frac43", "⁴⁄₃" },
    { "frac56", "⅚" },
    { "frac58", "⅝" },
    { "frac59", "⁵⁄₉" },
    { "frac67", "⁶⁄₇" },
    { "frac95", "⁹⁄₅" },
    { "frac1000x1434", "¹⁰⁰⁰⁄₁₄₃₄" },
    { "frac12x13",  "¹²⁄₁₃" },
    { "frac17x175", "¹⁷⁄₁₇₅" },
    { "frac1x10",   "⅒" },
    { "frac1x100",  "¹⁄₁₀₀" },
    { "frac1x1000", "¹⁄₁₀₀₀" },
    { "frac1x10000", "¹⁄₁₀₀₀₀" },
    { "frac1x108719", "¹⁄₁₀₈₇₁₉" },
    { "frac1x12",      "¹⁄₁₂" },
    { "frac1x20",   "¹⁄₂₀" },
    { "frac1x216000", "¹⁄₂₁₆₀₀₀" },
    { "frac1x24",   "¹⁄₂₄" },
    { "frac1x2500", "¹⁄₂₅₀₀" },
    { "frac1x29966", "¹⁄₂₉₉₆₆" },
    { "frac1x3600", "¹⁄₃₆₀₀" },
    { "frac1x50000", "¹⁄₅₀₀₀₀" },
    { "frac1x60",   "¹⁄₆₀" },
    { "frac1x6000", "¹⁄₆₀₀₀" },
    { "frac1x6400",  "¹⁄₆₄₀₀₀" },
    { "frac1x8000", "¹⁄₈₀₀₀" },
    { "frac25x100", "²⁵⁄₁₀₀" },
    { "frac2x10",   "²⁄₁₀" },
    { "frac3x16",   "³⁄₁₆" },
    { "frac925x1000", "⁹²⁵⁄₁₀₀₀" },

    { "?",      "<?>" }, /* Place-holder for unknown or illegible character. */
    { "hand",   "☞" },   /* pointing hand (printer's "fist") */
    { "nabla",  "∇" },
    { "sect",   "§" },
    { "amac",   "ā" },
    { "nsm",    "ṉ" },   /* "n sub-macron" */
    { "sharp",  "♯" },
    { "flat",   "♭" },
    { "th",     "th" },
    { "imac",   "ī" },
    { "emac",   "ē" },
    { "csdot",  "c̣"  },
    { "dsdot",  "ḍ" },   /* Sanskrit/Tamil d dot */
    { "hsdot",  "ḥ" },
    { "lsdot",  "ḷ" },
    { "msdot",  "ṃ" },
    { "nsdot",  "ṇ" },   /* Sanskrit/Tamil n dot */
    { "tsdot",  "ṭ" },   /* Sanskrit/Tamil t dot */
    { "zsdot",  "ẓ" },
    { "ecr",    "ĕ" },
    { "icr",    "ĭ" },
    { "ocr",    "ŏ" },
    { "OE",     "Œ" },
    { "oe",     "œ" },
    { "omac",   "ō" },
    { "Omac",   "Ō" },
    { "oomac",  "o͞o" },
    { "oocr",   "o͝o" },
    { "umac",   "ū" },
    { "ocar",   "ǒ" },
    { "aemac",  "ǣ" },
    { "ucr",    "ŭ" },
    { "acr",    "ă" },
    { "ycr",    "y̆" },
    { "ymac",   "ȳ" },
    { "asl",    "ā́" },   /* a "semilong" */
    { "esl",    "ḗ" },   /* e "semilong" */
    { "isl",    "ī́" },   /* i "semilong" */
    { "osl",    "ṓ" },   /* o "semilong" */
    { "usl",    "ū́" },   /* u "semilong" */
    { "adot",   "ȧ" },   /* a with dot above */
    { "edh",    "ð" },
    { "EDH",    "Ð" },
    { "thorn",  "þ" },
    { "atil",   "ã" },
    { "etil",   "ẽ" },
    { "itil",   "ĩ" },
    { "otil",   "õ" },
    { "util",   "ũ" },
    { "ntil",   "ñ" },
    { "ncir",   "ñ" },
    { "Atil",   "Ã" },
    { "Etil",   "Ẽ" },
    { "Itil",   "Ĩ" },
    { "Otil",   "Õ" },
    { "Util",   "Ũ" },
    { "Ntil",   "Ñ" },
    { "mdot",   "ṁ" },
    { "ndot",   "ṅ" },
    { "rsdot",  "ṛ" },
    { "usdot",  "ụ" },
    { "uring",  "ů" },
    { "zdot",   "ż" },
    { "yogh",   "ȝ" },
    { "deg",    "°" },
    { "min",    "′" },
    { "sec",    "˝" },
    { "middot", "•" },
    { "ounceap", "℥" },
    { "root",   "√" },
    { "cuberoot", "∛" },
    { "asterism", "⁂" },

    /* Music sheet notation */
    { "segno",  "𝄋" }, /* Introduced recently in entries for Segno, Al
			     segno, and Del segno. */
    { "pause",   "𝄐" },
    { "natural", "♮" },
    { "upslur",   "𝅷" },
    { "downslur", "𝅸" },

    /* Greek alphabet */
    { "alpha",    "α" },
    { "beta",     "β" },
    { "gamma",    "γ" },
    { "delta",    "δ" },
    { "epsilon",  "ε" },
    { "zeta",     "ζ" },
    { "eta",      "η" },
    { "theta",    "θ" },
    { "iota",     "ι" },
    { "kappa",    "κ" },
    { "lambda",   "λ" },
    { "mu",       "μ" },
    { "nu",       "ν" },
    { "xi",       "ξ" },
    { "omicron",  "ο" },
    { "pi",       "π" },
    { "rho",      "ρ" },
    { "sigma",    "σ" },
    { "sigmat",   "ς" },
    { "tau",      "τ" },
    { "upsilon",  "υ" },
    { "phi",      "φ" },
    { "chi",      "χ" },
    { "psi",      "ψ" },
    { "omega",    "ω" },
    { "digamma",  "ϝ" },
    { "ALPHA",    "Α" },
    { "BETA",     "Β" },
    { "GAMMA",    "Γ" },
    { "DELTA",    "Δ" },
    { "EPSILON",  "Ε" },
    { "ZETA",     "Ζ" },
    { "ETA",      "Η" },
    { "THETA",    "Θ" },
    { "IOTA",     "Ι" },
    { "KAPPA",    "Κ" },
    { "LAMBDA",   "Λ" },
    { "MU",       "Μ" },
    { "NU",       "Ν" },
    { "XI",       "Ξ" },
    { "OMICRON",  "Ο" },
    { "PI",       "Π" },
    { "RHO",      "Ρ" },
    { "SIGMA",    "Σ" },
    { "TAU",      "Τ" },
    { "UPSILON",  "Υ" },
    { "PHI",      "Φ" },
    { "CHI",      "Χ" },
    { "PSI",      "Ψ" },
    { "OMEGA",    "Ω" },
    /* Italic letters */
    { "AIT",      "A" },
    { "BIT",      "B" },
    { "CIT",      "C" },
    { "DIT",      "D" },
    { "EIT",      "E" },
    { "FIT",      "F" },
    { "GIT",      "G" },
    { "HIT",      "H" },
    { "IIT",      "I" },
    { "JIT",      "J" },
    { "KIT",      "K" },
    { "LIT",      "L" },
    { "MIT",      "M" },
    { "NOT",      "N" },
    { "OIT",      "O" },
    { "PIT",      "P" },
    { "QIT",      "Q" },
    { "RIT",      "R" },
    { "SIT",      "S" },
    { "TIT",      "T" },
    { "UIT",      "U" },
    { "VIT",      "V" },
    { "WIT",      "W" },
    { "XIT",      "X" },
    { "YIT",      "Y" },
    { "ZIT",      "Z" },
    { "ait",      "a" },
    { "bit",      "b" },
    { "cit",      "c" },
    { "dit",      "d" },
    { "eit",      "e" },
    { "fit",      "f" },
    { "git",      "g" },
    { "hit",      "h" },
    { "iit",      "i" },
    { "jit",      "j" },
    { "kit",      "k" },
    { "lit",      "l" },
    { "mit",      "m" },
    { "not",      "n" },
    { "oit",      "o" },
    { "pit",      "p" },
    { "qit",      "q" },
    { "rit",      "r" },
    { "sit",      "s" },
    { "tit",      "t" },
    { "uit",      "u" },
    { "vit",      "v" },
    { "wit",      "w" },
    { "xit",      "x" },
    { "yit",      "y" },
    { "zit",      "z" },

    { "add",      "a̤" },
    { "udd",      "ṳ" },
    { "ADD",      "A̤" },
    { "UDD",      "Ṳ" },

    /* Diacritics and special symbols */
    { "cre",      "⌣" },
    { "breve",    "̆" },
    { "umlaut",   "̈" },
    { "asper",    "̔" },
    { "prime",    "´" },
    { "Prime",    "′" },
    { "bprime",   "˝" },
    { "mdash",    "—" },
    { "divide",   "÷" },
    { "times",    "×" },

    /* Quotes */
    { "lsquo",    "‘" },
    { "ldquo",    "“" },
    { "rdquo",    "”" },

    { "dagger",   "†" },
    { "dag",      "†" },
    { "Dagger",   "‡" },
    { "ddag",     "‡" },
    { "para",     "§" },
    { "gt",       ">" },
    { "lt",       "<" },
    { "rarr",     "→" },
    { "larr",     "←" },
    { "schwa",    "ə" },

    { "br",       "\n" },
    { "and",      "and" },
    { "or",       "or" },

    { "iques",    "¿" },
    { "integral2l", "∫" },

    { "Aries",       "♈" },
    { "Taurus",      "♉" },
    { "Gemini",      "♊" },
    { "Cancer",      "♋" },
    { "Leo",         "♌" },
    { "Virgo",       "♍" },
    { "Libra",       "♎" },
    { "Scorpio",     "♏" },
    { "Sagittarius", "♐" },
    { "Capricorn",   "♑" },
    { "Aquarius",    "♒" },
    { "Pisces",      "♓" },

    { "Sun",         "☉" },
    { "Mercury",     "☿" },
    { "Venus",       "♀" },
    { "Mars",        "♂" },
    { "Jupiter",     "♃" },
    { "Uranus",      "♅" },
    { "Neptune",     "♆" },
    { "Ceres",       "⚳" },
    { "Chiron",      "⚷" },

    { "astascending",  "☊" },
    { "astdescending", "☋" },

    /* See CIDE.M, Mars, 3 */
    { "Male",        "♂" },

    /* Roman numerals */
    { "Crev",  "Ↄ" },


    { "filig", "ﬁ" },
    { "fllig", "ﬂ" },
    { "fflig", "ﬀ" },
    { "ffllig", "ﬄ" },

    /* In pronunciacions: */
    { "ai",    "ɑː" },

    { NULL }
};

char const *
gcide_entity_to_utf8(const char *str)
{
    struct gcide_entity *p;
    size_t len;

    if (str[0] == '<') {
	str++;
	len = strcspn(str, "/");
    } else
	len = strlen(str);

    for (p = gcide_entity; p->ent; p++) {
	if (p->ent[0] == str[0] && strlen(p->ent) == len && memcmp(p->ent, str, len) == 0)
	    return p->text;
    }
    return NULL;
}
