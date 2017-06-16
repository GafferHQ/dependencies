/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the demonstration applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
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
** $QT_END_LICENSE$
**
****************************************************************************/

varying vec3 position, normal;
varying vec4 specular, ambient, diffuse, lightDirection;

uniform sampler2D tex;

void main()
{
    vec3 N = normalize(normal);

    gl_MaterialParameters M = gl_FrontMaterial;

    // assume directional light
    float NdotL = dot(N, lightDirection.xyz);
    float RdotL = dot(reflect(normalize(position), N), lightDirection.xyz);

    float r1 = length(fract(7.0 * gl_TexCoord[1].xyz) - 0.5);
    float r2 = length(fract(5.0 * gl_TexCoord[1].xyz + 0.2) - 0.5);
    float r3 = length(fract(11.0 * gl_TexCoord[1].xyz + 0.7) - 0.5);
    vec4 rs = vec4(r1, r2, r3, 0.0);

    vec4 unlitColor = gl_Color * (0.8 - clamp(10.0 * (0.4 - rs), 0.0, 0.2));
    unlitColor.w = 1.0;
    gl_FragColor = (ambient + diffuse * max(NdotL, 0.0)) * unlitColor +
                    M.specular * specular * pow(max(RdotL, 0.0), M.shininess);
}
