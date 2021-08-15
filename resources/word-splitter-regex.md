**Whitespace Splitter Regex**

    `'s|'t|'re|'ve|'m|'l l|'d| ?\p{L}+| ?\p{N}+| ?[^\s\p{L}\p{N}]+|\s+(?!\S)|\s+`

* Match this alternative (attempting the next alternative only if this one fails) `'s`
    * Match the character string “'s” literally (case insensitive) `'s`
* Or match this alternative (attempting the next alternative only if this one fails) `'t`
    * Match the character string “'t” literally (case insensitive) `'t`
* Or match this alternative (attempting the next alternative only if this one fails) `'re`
    * Match the character string “'re” literally (case insensitive) `'re`
* Or match this alternative (attempting the next alternative only if this one fails) `'ve`
    * Match the character string “'ve” literally (case insensitive) `'ve`
* Or match this alternative (attempting the next alternative only if this one fails) `'m`
    * Match the character string “'m” literally (case insensitive) `'m`
* Or match this alternative (attempting the next alternative only if this one fails) `'ll`
    * Match the character string “'ll” literally (case insensitive) `'ll`
* Or match this alternative (attempting the next alternative only if this one fails) `'d`
    * Match the character string “'d” literally (case insensitive) `'d`
* Or match this alternative (attempting the next alternative only if this one fails) ` ?\p{L}+`
    * Match the character “ ” literally ` ?`
        * Between zero and one times, as many times as possible, giving back as needed (greedy) `?`
    * Match a character from the Unicode category “letter” (any kind of letter from any language) `\p{L}+`
        * Between one and unlimited times, as many times as possible, giving back as needed (greedy) `+`
* Or match this alternative (attempting the next alternative only if this one fails) ` ?\p{N}+`
    * Match the character “ ” literally ` ?`
        * Between zero and one times, as many times as possible, giving back as needed (greedy) `?`
    * Match a character from the Unicode category “number” (any kind of numeric character in any script) `\p{N}+`
        * Between one and unlimited times, as many times as possible, giving back as needed (greedy) `+`
* Or match this alternative (attempting the next alternative only if this one fails) ` ?[^\s\p{L}\p{N}]+`
    * Match the character “ ” literally ` ?`
        * Between zero and one times, as many times as possible, giving back as needed (greedy) `?`
    * Match any single character NOT present in the list below `[^\s\p{L}\p{N}]+`
        * Between one and unlimited times, as many times as possible, giving back as needed (greedy) `+`
        * A “whitespace character” (ASCII space, tab, line feed, carriage return, vertical tab, form feed) `\s`
        * A character from the Unicode category “letter” (any kind of letter from any language) `\p{L}`
        * A character from the Unicode category “number” (any kind of numeric character in any script) `\p{N}`
* Or match this alternative (attempting the next alternative only if this one fails) `\s+(?!\S)`
    * Match a single character that is a “whitespace character” (ASCII space, tab, line feed, carriage return, vertical tab, form feed) `\s+`
        * Between one and unlimited times, as many times as possible, giving back as needed (greedy) `+`
    * Assert that it is impossible to match the regex below starting at this position (negative lookahead) `(?!\S)`
        * Match a single character that is NOT a “whitespace character” (ASCII space, tab, line feed, carriage return, vertical tab, form feed) `\S`
* Or match this alternative (the entire match attempt fails if this one fails to match) `\s+`
    * Match a single character that is a “whitespace character” (ASCII space, tab, line feed, carriage return, vertical tab, form feed) `\s+`
        * Between one and unlimited times, as many times as possible, giving back as needed (greedy) `+`

The| Project| Gutenberg| eBook| of| Frankenstein|,| by| Mary| W|oll|stone|craft| (|God|win|)| Shelley| |This| eBook| is| for| the| use| of| anyone| anywhere| in| the| United| States| and| most| other| parts| of| the| world| at| no| cost| and| with| almost| no| restrictions| whatsoever|.| You| may| copy| it|,| give| it| away| or| re|-|use| it| under| the| terms| of| the| Project| Gutenberg| License| included| with| this| eBook| or| online| at| www|.|g|utenberg|.|org|.| If| you| are| not| located| in| the| United| States|,| you| will| have| to| check| the| laws| of| the| country| where| you| are| located| before| using| this| eBook|.| |Title|:| Frankenstein| |or|,| The| Modern| Prometheus| |Author|:| Mary| W|oll|stone|craft| (|God|win|)| Shelley| |Release| Date|:| 31|,| 1993| [|e|Book| #|84|]| |[|Most| recently| updated|:| November| 13|,| 2020|]| |Language|:| English| |Character| set| encoding|:| UTF|---|8| UTF|-|8| |Produ|ced| by|:| Judith| Boss|,| Christy| Phillips|,| Lynn| Hann|inen|,| and| David| Melt|zer|.| HTML| version| by| Al| H|ain|es|.| |Further| corrections| by| M|enn|o| de| Lee|u|w|.| | |***| START| OF| THE| PRO|JECT| G|UT|EN|BER|G| E|BOOK| FR|ANK|EN|STE|IN| ***| | |Frank|enstein|;| | |or|,| the| Modern| Prometheus| | |by| Mary| W|oll|stone|craft| (|God|win|)| Shelley| | |CONT|ENTS
The|ĠProject|ĠGutenberg|ĠeBook|Ġof|ĠFrankenstein|,|Ġby|ĠMary|ĠW|oll|stone|craft|Ġ(|God|win|)|ĠShelley|Ċ|This|ĠeBook|Ġis|Ġfor|Ġthe|Ġuse|Ġof|Ġanyone|Ġanywhere|Ġin|Ġthe|ĠUnited|ĠStates|Ġand|Ġmost|Ġother|Ġparts|Ġof|Ġthe|Ġworld|Ġat|Ġno|Ġcost|Ġand|Ġwith|Ġalmost|Ġno|Ġrestrictions|Ġwhatsoever|.|ĠYou|Ġmay|Ġcopy|Ġit|,|Ġgive|Ġit|Ġaway|Ġor|Ġre|-|use|Ġit|Ġunder|Ġthe|Ġterms|Ġof|Ġthe|ĠProject|ĠGutenberg|ĠLicense|Ġincluded|Ġwith|Ġthis|ĠeBook|Ġor|Ġonline|Ġat|Ġwww|.|g|utenberg|.|org|.|ĠIf|Ġyou|Ġare|Ġnot|Ġlocated|Ġin|Ġthe|ĠUnited|ĠStates|,|Ġyou|Ġwill|Ġhave|Ġto|Ġcheck|Ġthe|Ġlaws|Ġof|Ġthe|Ġcountry|Ġwhere|Ġyou|Ġare|Ġlocated|Ġbefore|Ġusing|Ġthis|ĠeBook|.|�|Title|:|ĠFrankenstein|Ċ|or|,|ĠThe|ĠModern|ĠPrometheus|Ċ|Author|:|ĠMary|ĠW|oll|stone|craft|Ġ(|God|win|)|ĠShelley|Ċ|Release|ĠDate|:|Ġ31|,|Ġ1993|Ġ[|e|Book|Ġ#|84|]|Ċ|�|[|Most|Ġrecently|Ġupdated|:|ĠNovember|Ġ13|,|Ġ2020|]|Ċ|�|Language|:|ĠEnglish|Ċ|Character|Ġset|Ġencoding|:|ĠUTF|---|8|ĠUTF|-|8|Ċ|Produ|ced|Ġby|:|ĠJudith|ĠBoss|,|ĠChristy|ĠPhillips|,|ĠLynn|ĠHann|inen|,|Ġand|ĠDavid|ĠMelt|zer|.|ĠHTML|Ġversion|Ġby|ĠAl|ĠH|ain|es|.|Ċ|�|Further|Ġcorrections|Ġby|ĠM|enn|o|Ġde|ĠLee|u|w|.|Ċ|�|Ċ|***|ĠSTART|ĠOF|ĠTHE|ĠPRO|JECT|ĠG|UT|EN|BER|G|ĠE|BOOK|ĠFR|ANK|EN|STE|IN|Ġ***|ĊĊ|Ċ|Frank|enstein|;|Ċ|�|Ċ|or|,|Ġthe|ĠModern|ĠPrometheus|Ċ|�|�|by|ĠMary|ĠW|oll|stone|craft|Ġ(|God|win|)|ĠShelley|Ċ|�|�|CONT|ENTS