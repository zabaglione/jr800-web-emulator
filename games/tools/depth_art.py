# SPDX-License-Identifier: MIT
"""Cell-contained depth cues. No collision, sprite dimensions or runtime drawing passes change."""
from art import Bitmap

def block8(kind='stone'):
    b=Bitmap(8,8)
    # Top and right faces of a low oblique block; the foot stays inside the logical cell.
    b.line(0,2,2,0);b.line(2,0,7,0);b.line(0,2,5,2);b.line(5,2,7,0)
    b.line(0,2,0,7);b.line(0,7,5,7);b.line(5,2,5,7);b.line(5,7,7,5);b.line(7,0,7,5)
    for y in range(2,6):b.dot(6,y)
    if kind=='crate':b.line(1,3,4,6);b.line(4,3,1,6)
    elif kind=='ice':b.line(1,5,3,3);b.dot(3,6)
    elif kind=='metal':b.dot(1,3);b.dot(4,6);b.line(1,5,3,5)
    else:b.line(0,5,5,5);b.line(2,3,2,5);b.dot(3,6)
    return b

def raised(b,left=0,top=0,right=None,bottom=None):
    right=b.w-1 if right is None else right;bottom=b.h-1 if bottom is None else bottom
    b.line(left,top+1,left,bottom-1);b.line(left+1,top,right-2,top)
    b.line(right-1,top+1,right-1,bottom-1);b.line(right,top+2,right,bottom)
    b.line(left+1,bottom-1,right-1,bottom-1);b.line(left+2,bottom,right,bottom)
    b.line(right-2,top,right,top+2);b.line(left,bottom-2,left+2,bottom)
    return b
