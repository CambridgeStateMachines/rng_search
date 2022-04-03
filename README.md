# rng_search
## 1. Summary
`rng_search` is a C project that demonstrates a fast parallel dictionary search technique enabling a target ASCII text stream to be searched for all instances of each of word in a dictionary, in parallel.

The dictionary of search terms is generated from a plain text file of ASCII characters containing one word per line, sorted alphabetically, and with any duplicates removed. The test text can be any plain text file of ASCII characters.

The examples in this repository use a dictionary, `wds.txt`, comprising 25,337 of the commonest words in English, using standard US spellings. The text used for benchmarking, `read_file.txt`, comprises the body text of the Wikipedia entry covering the presidency of Donald Trump, the [fourth longest Wikipedia article](https://en.wikipedia.org/wiki/Special:LongPages). When formatted as a plain text file, the test text is 172,506 bytes long. When opened in Microsoft Word 365, and formatted in 11 point Ariel with 1.5 paragraph spacing, the resultant document is 68 pages long, and contains 25,913 words.

## 2. Benchmarks
Using the `wds.txt` file as the dictionary source (25,337 words), and the `read_text.txt` file as the target text (25,913 words) `rng_search` finds 22,270 hits.

On my medium performance ASUS laptop (AMD Ryzen 5 4500U with Radeon Graphics, 8GB RAM, 2375 Mhz, 6 Core(s), 6 Logical Processor(s), running Windows 11), the average elapsed times over multiple runs were as follows:

- Generating the `ptn` and `lens` arrays from `wds.txt`, and then processing `read_file.txt`: 32ms
- Loading pre-saved copies of the `ptn` and `lens` arrays from the files `ptn.dat` and `lens.dat` into memory, and then processing `read_file.txt`: 4ms

The source code compiles and runs under GNU/Linux on low performance hardware (Pi Zero 2 W, Broadcom BCM2835 ARMv7 Processor rev 4 (v7l), 1GHZ, 512MB RAM, running Raspbian GNU/Linux 11), albeit much more slowly: around 260ms to generate `ptn` and `lens` from `wds.txt` before processing `read_file.txt`, and 84ms with pre-saved `ptn` and `lens`.

In tests, these speed figures compare favourably with the Aho-Corasick algorithm based on the same dictionary and test text.

I believe that this is principally because the amount of memory required to contain `ptn` and `lens` is far smaller than the memory required to contain the corresponding deterministic finite state machine ("DFSM") used in Aho-Corasick.

Experiments suggest that, for an extended ASCII alphabet, the DFSM derived from `wd.txt` has 63,604 states. Assuming 32 bit integers, and allowing for an additional three columns for the "Length", "Out" and "Fail" parameters used by the algorithm, the total memory requirement is 65,893,744 bytes, compared with a total of 1,122,696 bytes for `rng_search` (1,021,344 bytes for the 63,834 PTNs of `ptn`, and 101,352 bytes for the 25,377 words in `lens`), which is less than 2% of the memory requirement.

## 3. Custom structs
`rng_search` relies on a custom pattern struct ("PTN"), described below:

```
typedef struct {
    unsigned char ch; //an ASCII character
    unsigned int prv; //index of the previous PTN in a linked list within an array of PTNs
    unsigned int nxt; //index of the next PTN in a linked list within an array of PTNs
    unsinged int sta; //the start index of a position-specific sub-range of word indices within the dictionary
} PTN;
```
The dictionary can, when sorted alphabetically, be viewed as a collection of nested "ranges" of consecutive word indices, with one level of nesting for each character position. For example, the range of indices of words in the dictionary with the initial letter "g" will start at a given index, sta<sub>g</sub>, and will comprise len<sub>g</sub> consecutive words.

Thus, for the initial position, the parameters for each range will be:

- sta<sub>x</sub> = the index of the first word in the dictionary whose first character is x
- len<sub>x</sub> = the number of words in the dictionary whose first character is x

For the second position, the parameters of each range define a sub-range of its parent range:

- sta<sub>xy</sub> = the index of the first word whose first character is x, and second character is y
- len<sub>xy</sub> = the number of words whose first character is x, and second character is y

Likewise, for the third position:

- sta<sub>xyz</sub> = the index of the first word whose first three characters are xyz
- len<sub>xyz</sub> = the number of words whose first three characters are xyz

and so on.

Obviously, the `len` parameter of each sub-range of a range cannot be greater that the `len` parameter of the parent range.

Because multiple words in the dictionary have common prefixes, the number of ranges required to define the whole dictionary is greater than the number of words, but substantially fewer than the combined number of characters in all of the words in the dictionary.

For example, 63,833 ranges are required to cover the dictionary of 25,337 words used in this project. This is about 250% of the number of words in the dictionary, and 35% of the total number of alphabetical characters in the dictionary (185,351). These ratios appear to be reasonably consistent across multiple English language dictionaries. For example, a dictionary with 127,143 words and 1,176,874 characters required 264,405 ranges (208% and 22%). A dictionary with 370,103 words and 4,234,903 characters required 1,028,043 ranges (278% and 24%).

The PTN struct keeps track of the `sta` parameter of the ranges and sub-ranges that make up the dictionary. The `len` parameter is not required. The PTN struct contains three further elements: an unsigned char, `ch`, corresponding to the ASCII character that defines the relevant sub-range, and two unsigned ints, `prv` and `nxt`, which define, respectively, the previous and next PTNs in the linked list that is formed by the array of PTNs generated from the dictionary. For the first letter, the `prv` parameter is set to 0. The role played by each element of the PTN construct is explained in the paragraphs below.

## 4. Generating the PTN array from the dictionary
The first step in preparing the PTN array from the dictionary text file is to convert the file into a two dimensional dynamic array of type unsigned char, `**wd_mtrx`, using standard techniques, see the project code for one example. The number of rows is set to the number of words in the dictionary, and the number of columns is set to a constant, MAX_LEN, that defines the maximum permissible length of words in the dictionary. In this project the value of MAX_LEN is set to 32.

Once the number of characters in the dictionary text file is known, a dynamic one dimensional array of PTN, `*ptn`, with the number of items set to the number of characters in the dictionary text file, is generated and zeroed. This will be the array that is returned (or saved) by the dictionary generating function having been realloc'd to reduce its size to the actual number of PTNs arising from steps two and three.

Step two is the generation of a two dimensional dynamic array of type unsigned int, `**wd_int_mtrx`, with the same number of rows and columns as `**wd_mtrx`. The contents of `**wd_int_mtrx` are zeroed.

The third step is to traverse `**wd_mtrx` column-by-column and row-by-row, incrementing a counting parameter, `ptn_count`, each time the sub-string defined up to `wd_mtrx[row][column]` changes, and/or a new column is started, writing the current value of `ptn_count` to `wd_int_mtrx[row][column]`, and adding a new PTN to `ptn`. The value of `sta` for the new PTN is set to the value of `row` (which is the index of the current word).

For the second and subsequent columns, the value of `prv` for each new PTN initated by a change of sub-string up to `wd_mtrx[row][column]` is set to the value of `wd_int_mtrx[row][column - 1]`. Likewise, the value of `nxt` for `ptn[wd_int_mtrx[row][column - 1]]` is set to `wd_int_mtrx[row][column]`.

Finally, the size of `ptn` is realloc'd to `ptn_count * sizeof(*ptn)`.

The total size of the PTN array generated from the dictionary must be A + B, where A = the size of the alphabet used by the dictionary, and B = the total number of PTNs covering positions 2 to MAX_LEN.

In this project, the alphabet is the extended ASCII character set, so A = 256. When traversing column 0 of `wd_mtrx`, therefore, the index of each new PTN is simply set to the value of `ch` e.g. the index of the PTN for words with the initial letter 'a' is 97. For the second column in the traverse of `wd_mtrx`, the value of `ptn_count` starts at 256.

The PTN array generation steps are summarised below in pseudocode:
```
ALPHABET = 256 //extended ASCII
MAX_LEN = 32 //maximum length of words in the dictionary

get_wd_ptn(wd_mtrx[wd_count][MAX_LEN])
{
    ptn_count = ALPHABET
    Initialise ptn[ALPHABET + (wd_count * MAX_LEN)], and zero its contents
    Initialise wd_int_mtrx[wd_count][MAX_LEN], and zero its contents
    for position = 0 to MAX_LEN
        for word_id = 1 to wd_count
            if position == 0, process the first column differently from the second and subsequent columns:
                if this is the first time the letter at wd_mtrx[word_id][0] has been encountered at position 0:
                    set the ch parameter of the current ptn to the letter at wd_mtrx[word_id][0]
                    set the sta parameter of the current ptn to the current word_id
                set the index of of the current ptn to the ASCII value of the letter at wd_mtrx[word_id][0]
            else
                if the value of wd_mtrx[word_id][column] is non-zero:
                    if the first [position + 1] characters of the current row != the first [position + 1] characters of the previous row
                        populate int_mtrx[word_id][position] with the current value of ptn_count 
                        ch value = ASCII value of the letter at wd_mtrx[word_id][position]
                        prv value = the value of int_mtrx[word_id][position - 1]
                        nxt = the current ptn_count
                        sta = word_id
                        increment ptn_count
                    else
                        int_mtrx[word_id][position] = int_mtrx[word_id - 1][position]
    reallocate ptn to size = ptn_count * sizeof(*ptn)
    return ptn
}
```
In addition to `ptn`, the search algorithm requires a dynamic one dimensional array of type unsigned int, `*wd_lens`, with a length equal to the number of words in the dictionary text, to hold the length of each word. This array can be generated very easily using standard techniques, see the project code for one example.

If the dictionary is intended to be reused, `ptn` and `lens` can be generated once at leisure and saved to binary files. They can then be loaded into memory as required, cutting out the dictionary generation steps. See project code for examples of save and load functions for `ptn` and `lens`.

## 5. The search algorithm
Once `ptn` has been generated (or loaded into memory from a file), the search algorithm is relatively simple.

Assuming that only whole word matches are sought, the first step is to advance the value of the `txt_pos` variable to the index of the first character of the search text for which the value of `ptn[txt[txt_pos]].sta` is non-zero. If, as in the example dictionary, `wds.txt`, contains only lowercase alphabetical characters, then the value of `txt_pos` can be advanced until the value of `ptn[tolower(txt[txt_pos])].nxt` is non-zero.

The value of the `wd_pos` variable is then set to 1, and the program enters an infinite while loop which will continue until the matching algorithm fails; if an exact match to a word in the dictionary has been found, its index and location are the values of the `ptn[prev_ptn].sta` and `curr_sta` parameters passed to the hit test function respectively - see project code.

Within the infinite while loop, if the value of `wd_pos` is 1, the value of `curr_sta` is set to the same value as `txt_pos`, the value of `prev_ptn` is set to 0, and the value of `curr_ptn` is set to the ASCII value of the character at `txt[txt_pos]`. If the value of `wd_pos` is greater than 1, the members of the subset of PTNs whose `prev` parameter is the same as the current value of `prev_ptn` are consecutively tested for whether their `ch` property matches the character at `txt[txt_pos]` by iterating the `curr_ptn` variable. Therefore, although the algorithm considers each character in the target text only once, multiple PTNs may be considered per character until either a match is found or the infinite loop exits. Tests suggest that the ratio of PTN reads to target text characters is around 2.5. Obviously, sorting the dictionary to place more popular suffixes higher in sub-ranges will reduce the number of PTN reads. Tests suggest that, re-sorting the dictionary by ascending initial letter, and descending frequency of just the first two letters of words with the same initial letter, the ratio of PTN reads to target text characters is reduced to around 2.2.

- If a match is found on or before the last PTN whose `prev` parameter is the same as the current value of `prev_ptn`, the index of the next PTN to be considered is identified from the `nxt` parameter of the current PTN. The values of `prev_ptn` and `curr_ptn` are updated to the values of `curr_ptn` and `ptn[curr_ptn].nxt` respectively, and the infinite while loop continues.
- If no such match is found, the current word is not in the dictionary, and the infinite while loop is exited. The value of `txt_pos` is incremented until `isalpha[txt[txt_pos]]` is non-zero i.e. until the end of the current word

At the end of each iteration of the infinite while loop, the current values of the parameters are tested to see whether a hit has been found. Iff the value of `lens[ptn[prev_ptn].sta]` matches the `wd_pos` parameter AND the value of `isalpha[txt[txt_pos + 1]]` is non-zero, a match with word index `ptn[prev_ptn].sta` is recorded at the location of `curr_sta`. The values of `txt_pos` and `wd_pos` are incremented prior to the next iteration.

Note that the `wd_id` argument that is passed to the hit checking function is equal to the value of the `ptn[prev_ptn].sta`. It is a property of this algorithm that hits are only recorded if the dictionary index of the word that is found corresponds to the `sta` value of the sub-range of the current PTN.

The search algorithm steps are summarised below in pseudocode:
```
proc_wd(test_txt, file_size, *ptn, *lens)
{
    txt_pos = 0
    while txt_pos < file_size
        while isalpha(test_txt[txt_pos]) is zero, increment txt_pos
        wd_pos = 1
        while (1)
            if wd_pos == 1
                curr_sta = txt_pos
                prev_ptn = test_txt[txt_pos]
                curr_ptn = ptn[test_txtt[txt_pos]].nxt
            else
                while ptn[curr_ptn].prv == prev_ptn AND ptn[curr_ptn].ch != test_txt[txt_pos]) increment curr_ptn
                if ptn[curr_ptn].prv != prev_ptn, break
                prev_ptn = curr_ptn
                curr_ptn = ptn[curr_ptn].nxt
            }
            get_wd_hit(test_txt, txt_pos, curr_sta, wd_pos, ptn[prev_ptn].sta, lens[ptn[prev_ptn].sta])
            increment txt_pos and wd_pos
        }
        while isalpha(txt[txt_pos])) is non-zero, increment txt_pos
    }
}

get_wd_hit(test_txt, txt_pos, curr_sta, wd_pos, wd_id, wd_len)
{
    if isalpha(test_txt[txt_pos + 1]) is non-zero AND wd_pos == wd_len
        record hit with position = curr_sta, and dictionary index = wd_id
    }
}
```
