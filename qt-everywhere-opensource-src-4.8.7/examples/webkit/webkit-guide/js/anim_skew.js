/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt WebKit module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/
var app = new Function();

app.init = function() {
    app.elements = document.querySelectorAll('.items > div');
    app.navs = document.querySelectorAll('nav > div');

    // elements
    for ( var i = 0, ln = app.elements.length ; i < ln ; i++ ) {
        app.elements[i].className = 'row' + (i + 1);
        app.elements[i].addEventListener('click', app.remove);
    }

    // navigation
    for ( var i = 0, ln = app.navs.length ; i < ln ; i++ ) {
        app.navs[i].addEventListener('click', app.filter );
    }
};

app.filter = function(event) {
    var type = event.target.className;

    var hiddenCount = 0;

    if (! type ) {
        app.init();
        return false;
    }

    for ( var i = 0, ln = app.elements.length ; i < ln ; i++ ) {
        if ( app.elements[i].title == type ) {
            app.elements[i].className = 'row' + ((i + 1) - hiddenCount);
        }
        else {
            app.elements[i].className = 'hide';
            hiddenCount++;
        }
    }
};

app.remove = function() {
    event.currentTarget.className = 'hide';
    app.rearrange();
};

app.rearrange = function() {
    var hiddenCount = 0;
    for ( var i = 0, ln = app.elements.length ; i < ln ; i++ ) {
        if ( app.elements[i].className.match(/hide/) ) {
            hiddenCount++;
        }
    else {
            app.elements[i].className = 'row' + ((i + 1) - hiddenCount);
        }
    }
};

window.onload = function() { app.init() };

