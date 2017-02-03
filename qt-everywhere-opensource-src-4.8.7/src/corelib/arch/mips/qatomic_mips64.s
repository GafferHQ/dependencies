;/****************************************************************************
;**
;** Copyright (C) 2015 The Qt Company Ltd.
;** Contact: http://www.qt.io/licensing/
;**
;** This file is part of the QtGui module of the Qt Toolkit.
;**
;** $QT_BEGIN_LICENSE:LGPL$
;** Commercial License Usage
;** Licensees holding valid commercial Qt licenses may use this file in
;** accordance with the commercial license agreement provided with the
;** Software or, alternatively, in accordance with the terms contained in
;** a written agreement between you and The Qt Company. For licensing terms
;** and conditions see http://www.qt.io/terms-conditions. For further
;** information use the contact form at http://www.qt.io/contact-us.
;**
;** GNU Lesser General Public License Usage
;** Alternatively, this file may be used under the terms of the GNU Lesser
;** General Public License version 2.1 as published by the Free Software
;** Foundation and appearing in the file LICENSE.LGPL included in the
;** packaging of this file.  Please review the following information to
;** ensure the GNU Lesser General Public License version 2.1 requirements
;** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
;**
;** As a special exception, The Qt Company gives you certain additional
;** rights. These rights are described in The Qt Company LGPL Exception
;** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
;**
;** GNU General Public License Usage
;** Alternatively, this file may be used under the terms of the GNU
;** General Public License version 3.0 as published by the Free Software
;** Foundation and appearing in the file LICENSE.GPL included in the
;** packaging of this file.  Please review the following information to
;** ensure the GNU General Public License version 3.0 requirements will be
;** met: http://www.gnu.org/copyleft/gpl.html.
;**
;**
;** $QT_END_LICENSE$
;**
;****************************************************************************/
	.set nobopt
	.set noreorder
	.option pic2
	.text

	.globl	q_atomic_test_and_set_int
	.ent	q_atomic_test_and_set_int
q_atomic_test_and_set_int:
1:	ll   $8,0($4)
	bne  $8,$5,2f
	move $2,$6
	sc   $2,0($4)
	beqz $2,1b
	nop
	jr   $31
	nop
2:	jr   $31
	move $2,$0
	.end	q_atomic_test_and_set_int

	.globl	q_atomic_test_and_set_acquire_int
	.ent	q_atomic_test_and_set_acquire_int
q_atomic_test_and_set_acquire_int:
1:	ll   $8,0($4)
	bne  $8,$5,2f
	move $2,$6
	sc   $2,0($4)
	beqz $2,1b
	nop
	jr   $31
	nop
2:	sync
	jr   $31
	move $2,$0
	.end	q_atomic_test_and_set_acquire_int

	.globl	q_atomic_test_and_set_release_int
	.ent	q_atomic_test_and_set_release_int
q_atomic_test_and_set_release_int:
	sync
1:	ll   $8,0($4)
	bne  $8,$5,2f
	move $2,$6
	sc   $2,0($4)
	beqz $2,1b
	nop
	jr   $31
	nop
2:	jr   $31
	move $2,$0
	.end	q_atomic_test_and_set_release_int

	.globl	q_atomic_test_and_set_ptr
	.ent	q_atomic_test_and_set_ptr
q_atomic_test_and_set_ptr:
1:	lld  $8,0($4)
	bne  $8,$5,2f
	move $2,$6
	scd  $2,0($4)
	beqz $2,1b
	nop
	jr   $31
	nop
2:	jr   $31
	move $2,$0
	.end	q_atomic_test_and_set_ptr

	.globl	q_atomic_test_and_set_acquire_ptr
	.ent	q_atomic_test_and_set_acquire_ptr
q_atomic_test_and_set_acquire_ptr:
1:	lld  $8,0($4)
	bne  $8,$5,2f
	move $2,$6
	scd  $2,0($4)
	beqz $2,1b
	nop
	jr   $31
	nop
2:	sync
	jr   $31
	move $2,$0
	.end	q_atomic_test_and_set_acquire_ptr
	
	.globl	q_atomic_test_and_set_release_ptr
	.ent	q_atomic_test_and_set_release_ptr
q_atomic_test_and_set_release_ptr:
	sync
1:	lld  $8,0($4)
	bne  $8,$5,2f
	move $2,$6
	scd  $2,0($4)
	beqz $2,1b
	nop
	jr   $31
	nop
2:	jr   $31
	move $2,$0
	.end	q_atomic_test_and_set_release_ptr
