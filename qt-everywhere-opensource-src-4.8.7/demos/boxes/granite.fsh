/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the demonstration applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

varying vec3 position, normal;
varying vec4 specular, ambient, diffuse, lightDirection;

uniform sampler2D tex;
uniform sampler3D noise;

//const vec4 graniteColors[3] = {vec4(0.0, 0.0, 0.0, 1), vec4(0.30, 0.15, 0.10, 1), vec4(0.80, 0.70, 0.75, 1)};
uniform vec4 graniteColors[3];

float steep(float x)
{
    return clamp(5.0 * x - 2.0, 0.0, 1.0);
}

void main()
{
    vec2 turbulence = vec2(0, 0);
    float scale = 1.0;
    for (int i = 0; i < 4; ++i) {
        turbulence += scale * (texture3D(noise, gl_TexCoord[1].xyz / scale).xy - 0.5);
        scale *= 0.5;
    }

    vec3 N = normalize(normal);
    // assume directional light

    gl_MaterialParameters M = gl_FrontMaterial;

    float NdotL = dot(N, lightDirection.xyz);
    float RdotL = dot(reflect(normalize(position), N), lightDirection.xyz);

    vec4 unlitColor = mix(graniteColors[1], mix(graniteColors[0], graniteColors[2], steep(0.5 + turbulence.y)), 4.0 * abs(turbulence.x));
    gl_FragColor = (ambient + diffuse * max(NdotL, 0.0)) * unlitColor +
                    M.specular * specular * pow(max(RdotL, 0.0), M.shininess);
}
