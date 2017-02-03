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
	.set noreorder
	.set volatile
	.set noat
	.arch ev4
	.text
	.align 2
	.align 4
	.globl q_atomic_test_and_set_int
	.ent q_atomic_test_and_set_int
q_atomic_test_and_set_int:
	.frame $30,0,$26,0
	.prologue 0
1:	ldl_l $0,0($16)
	cmpeq $0,$17,$0
	beq   $0,3f
	mov   $18,$0
	stl_c $0,0($16)
	beq   $0,2f
	br    3f
2:	br    1b
3:	addl  $31,$0,$0
	ret   $31,($26),1
	.end q_atomic_test_and_set_int
	.align 2
	.align 4
	.globl q_atomic_test_and_set_acquire_int
	.ent q_atomic_test_and_set_acquire_int
q_atomic_test_and_set_acquire_int:
	.frame $30,0,$26,0
	.prologue 0
1:	ldl_l $0,0($16)
	cmpeq $0,$17,$0
	beq   $0,3f
	mov   $18,$0
	stl_c $0,0($16)
	beq   $0,2f
	br    3f
2:	br    1b
3:	mb
	addl  $31,$0,$0
	ret   $31,($26),1
	.end q_atomic_test_and_set_acquire_int
	.align 2
	.align 4
	.globl q_atomic_test_and_set_release_int
	.ent q_atomic_test_and_set_release_int
q_atomic_test_and_set_release_int:
	.frame $30,0,$26,0
	.prologue 0
	mb
1:	ldl_l $0,0($16)
	cmpeq $0,$17,$0
	beq   $0,3f
	mov   $18,$0
	stl_c $0,0($16)
	beq   $0,2f
	br    3f
2:	br    1b
3:	addl  $31,$0,$0
	ret   $31,($26),1
	.end q_atomic_test_and_set_release_int
	.align 2
	.align 4
	.globl q_atomic_test_and_set_ptr
	.ent q_atomic_test_and_set_ptr
q_atomic_test_and_set_ptr:
	.frame $30,0,$26,0
	.prologue 0
1:	ldq_l $0,0($16)
	cmpeq $0,$17,$0
	beq   $0,3f
	mov   $18,$0
	stq_c $0,0($16)
	beq   $0,2f
	br    3f
2:	br    1b
3:	addl  $31,$0,$0
	ret   $31,($26),1
	.end q_atomic_test_and_set_ptr
	.align 2
	.align 4
	.globl q_atomic_increment
	.ent q_atomic_increment
q_atomic_increment:
	.frame $30,0,$26,0
	.prologue 0
1:	ldl_l $0,0($16)
	addl  $0,1,$1
	stl_c $1,0($16)
	beq   $1,2f
	br    3f
2:	br    1b
3:	addl $31,$0,$0
	cmpeq $0,$1,$0
	xor $0,1,$0
	ret $31,($26),1
	.end q_atomic_increment
	.align 2
	.align 4
	.globl q_atomic_decrement
	.ent q_atomic_decrement
q_atomic_decrement:
	.frame $30,0,$26,0
	.prologue 0
1:	ldl_l $0,0($16)
	subl  $0,1,$1
	stl_c $1,0($16)
	beq   $1,2f
	br    3f
2:	br    1b
3:	addl  $31,$0,$0
	cmpeq $0,1,$0
	xor $0,1,$0
	ret $31,($26),1
	.end q_atomic_decrement
	.align 2
	.align 4
	.globl q_atomic_set_int
	.ent q_atomic_set_int
q_atomic_set_int:
	.frame $30,0,$26,0
	.prologue 0
1:	ldl_l $0,0($16)
	mov   $17,$1
	stl_c $1,0($16)
	beq   $1,2f
	br    3f
2:	br    1b
3:	addl  $31,$0,$0
	ret   $31,($26),1
	.end q_atomic_set_int
	.align 2
	.align 4
	.globl q_atomic_set_ptr
	.ent q_atomic_set_ptr
q_atomic_set_ptr:
	.frame $30,0,$26,0
	.prologue 0
1:	ldq_l $0,0($16)
	mov   $17,$1
	stq_c $1,0($16)
	beq   $1,2f
	br    3f
2:	br    1b
3:	ret   $31,($26),1
	.end q_atomic_set_ptr

	.align 2
	.align 4
	.globl q_atomic_fetch_and_add_int
	.ent q_atomic_fetch_and_add_int
q_atomic_fetch_and_add_int:
	.frame $30,0,$26,0
	.prologue 0
1:	ldl_l $0,0($16)
	addl  $0,$17,$1
	stl_c $1,0($16)
	beq   $1,2f
	br    3f
2:	br    1b
3:	addl $31,$0,$0
	ret $31,($26),1
	.end q_atomic_fetch_and_add_int

	.align 2
	.align 4
	.globl q_atomic_fetch_and_add_acquire_int
	.ent q_atomic_fetch_and_add_acquire_int
q_atomic_fetch_and_add_acquire_int:
	.frame $30,0,$26,0
	.prologue 0
1:	ldl_l $0,0($16)
	addl  $0,$17,$1
	stl_c $1,0($16)
	beq   $1,2f
	br    3f
2:	br    1b
3:	mb
        addl $31,$0,$0
	ret $31,($26),1
	.end q_atomic_fetch_and_add_acquire_int

	.align 2
	.align 4
	.globl q_atomic_fetch_and_add_release_int
	.ent q_atomic_fetch_and_add_release_int
q_atomic_fetch_and_add_release_int:
	.frame $30,0,$26,0
	.prologue 0
        mb
1:	ldl_l $0,0($16)
	addl  $0,$17,$1
	stl_c $1,0($16)
	beq   $1,2f
	br    3f
2:	br    1b
3:      addl $31,$0,$0
	ret $31,($26),1
	.end q_atomic_fetch_and_add_release_int
