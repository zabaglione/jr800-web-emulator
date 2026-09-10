// SPDX-License-Identifier: MIT
// Generic catalog entry loading. Game rules never cross the WASM boundary.
import {MAX_LINKED_BINARY_BYTES} from './wasm-machine.mjs';
export function requestedProgram(search){
    const id=new URLSearchParams(search).get('program');
    if(id===null)return null;
    if(!/^[a-z][a-z0-9-]{0,39}$/.test(id))throw new Error('Invalid program ID');
    return id;
}
export async function fetchCatalogProgram(id,base=import.meta.url){
    const response=await fetch(new URL('./program-catalog.json',base),{mode:'same-origin'});
    if(!response.ok)throw new Error('Program catalog is unavailable');
    const catalog=await response.json();
    if(catalog.version!==1||!Array.isArray(catalog.programs))throw new Error('Invalid program catalog');
    const matches=catalog.programs.filter(p=>p.id===id);
    if(matches.length!==1)throw new Error('Unknown program ID');
    const entry=matches[0];
    if(entry.file!==`${id}.j8a`||! /^[0-9a-f]{64}$/.test(entry.sha256)||typeof entry.title!=='string')throw new Error('Invalid program entry');
    const result=await fetch(new URL(`./${entry.file}`,base),{mode:'same-origin'});
    if(!result.ok)throw new Error('Program download failed');
    const data=await result.arrayBuffer();
    if(!data.byteLength||data.byteLength>MAX_LINKED_BINARY_BYTES)throw new Error('Invalid program size');
    const digest=Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',data)),b=>b.toString(16).padStart(2,'0')).join('');
    if(digest!==entry.sha256)throw new Error('Program integrity check failed');
    return {entry,data};
}
