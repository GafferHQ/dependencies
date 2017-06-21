
/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef ForthParser_DEFINED
#define ForthParser_DEFINED

#include "SkTDict.h"
//#include "SkString.h"

class ForthWord;
class FCode;

class ForthParser {
public:
    ForthParser();
    ~ForthParser();

    const char* parse(const char text[], FCode*);

    void addWord(const char name[], ForthWord* word) {
        this->add(name, strlen(name), word);
    }

    void add(const char name[], size_t len, ForthWord* word) {
    //    SkString str(name, len);
    //    SkDebugf("add %s %p\n", str.c_str(), word);
        SkDEBUGCODE(bool isNewWord = )fDict.set(name, len, word);
        SkASSERT(isNewWord);
    }

    ForthWord* find(const char name[], size_t len) const {
        ForthWord* word;
        return fDict.find(name, len, &word) ? word : NULL;
    }

private:
    void addStdWords();

    SkTDict<ForthWord*> fDict;
};

#endif
