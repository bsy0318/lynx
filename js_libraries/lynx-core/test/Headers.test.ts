// Copyright 2026 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { describe, expect, test } from 'vitest';

import { Response } from '../src/modules/fetch/Response';

function createHeadersFromNativeResponse(headers: [string, string][]) {
  return new Response(null, { headers }).headers;
}

describe('Headers', () => {
  test('looks up mixed-case native response headers case-insensitively', () => {
    const headers = createHeadersFromNativeResponse([
      ['Link', '<https://example.com/next>; rel="next"'],
      ['Content-Type', 'application/json'],
    ]);

    expect(headers.get('link')).toBe(headers.get('Link'));
    expect(headers.get('LINK')).toBe(headers.get('Link'));
    expect(headers.get('content-type')).toBe('application/json');
    expect(headers.has('CONTENT-TYPE')).toBe(true);
  });

  test('normalizes names consistently for mutations', () => {
    const headers = createHeadersFromNativeResponse([['X-Page', 'first']]);

    headers.append('x-PAGE', 'second');
    expect(headers.get('X-page')).toBe('first, second');
    expect([...headers.entries()]).toEqual([['x-page', 'first, second']]);

    headers.set('X-PAGE', 'replacement');
    expect(headers.get('x-page')).toBe('replacement');
    expect(headers.has('x-PaGe')).toBe(true);

    headers.delete('X-page');
    expect(headers.has('x-page')).toBe(false);
    expect(headers.get('X-PAGE')).toBeNull();
  });

  test('combines duplicate names regardless of case', () => {
    const headers = createHeadersFromNativeResponse([
      ['Warning', '199 first'],
      ['warning', '299 second'],
    ]);

    expect(headers.get('WARNING')).toBe('199 first, 299 second');
    expect([...headers.keys()]).toEqual(['warning']);
  });

  test('iterates normalized names in sorted order', () => {
    const headers = createHeadersFromNativeResponse([
      ['name1', 'value1'],
      ['Name2', 'value2'],
      ['name', 'value3'],
      ['content-Type', 'value4'],
      ['Content-Typ', 'value5'],
      ['Content-Types', 'value6'],
    ]);

    expect([...headers.keys()]).toEqual([
      'content-typ',
      'content-type',
      'content-types',
      'name',
      'name1',
      'name2',
    ]);
    expect([...headers.values()]).toEqual([
      'value5',
      'value4',
      'value6',
      'value3',
      'value1',
      'value2',
    ]);
  });

  test('normalizes leading and trailing HTTP whitespace in values', () => {
    const headers = createHeadersFromNativeResponse([
      ['name1', ' space '],
      ['name2', '\ttab\t'],
      ['name3', ' spaceAndTab\t'],
      ['name4', '\r\n newLine'],
      ['name5', 'newLine\r\n '],
      ['name6', '\r\n\tnewLine'],
      ['name7', '\t\f\tnewLine\n'],
      ['name8', 'newLine\xa0'],
    ]);

    expect(headers.get('name1')).toBe('space');
    expect(headers.get('name2')).toBe('tab');
    expect(headers.get('name3')).toBe('spaceAndTab');
    expect(headers.get('name4')).toBe('newLine');
    expect(headers.get('name5')).toBe('newLine');
    expect(headers.get('name6')).toBe('newLine');
    expect(headers.get('name7')).toBe('\f\tnewLine');
    expect(headers.get('name8')).toBe('newLine\xa0');
  });

  test('normalizes appended and set values', () => {
    const headers = createHeadersFromNativeResponse([]);

    headers.append('x-test', ' first\t');
    headers.append('X-Test', '\r\n second ');
    expect(headers.get('x-test')).toBe('first, second');

    headers.set('x-test', '\t replacement\n');
    expect(headers.get('x-test')).toBe('replacement');
  });
});
