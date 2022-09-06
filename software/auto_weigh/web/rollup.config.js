import { DEFAULT_EXTENSIONS } from '@babel/core';
import babel from '@rollup/plugin-babel';
import typescript from 'rollup-plugin-typescript2';
import commonjs from '@rollup/plugin-commonjs';
import postcss from 'rollup-plugin-postcss';
import import_url from 'postcss-import-url';
import font_grabber from 'postcss-font-grabber';
import autoprefixer from 'autoprefixer';
import resolve from '@rollup/plugin-node-resolve';
import url from '@rollup/plugin-url';
import svgr from '@svgr/rollup';
import { terser } from 'rollup-plugin-terser';
import replace from '@rollup/plugin-replace';
import eslint from '@rollup/plugin-eslint';
import copy from 'rollup-plugin-copy'
import pkg from './package.json';

import path from 'path';

const is_prod = process.env.NODE_ENV == 'production';
const NODE_ENV = is_prod ? "\'production\'" : "\'development\'";
const fonts_dir = path.resolve('dist/fonts');
const html_dir = path.resolve('dist/html');

export default {
  input: ['src/js/index.tsx'],
  output: [{
    sourcemap: true,
    dir: 'dist/js',
    format: 'esm',
  }],
  plugins: [
    eslint(),
    copy({
      targets: [
        { src: 'src/favicon.ico', dest: 'dist' },
        { src: 'src/html/*.html', dest: 'dist/html' },
      ],
    }),
    replace({
       'process.env.NODE_ENV': NODE_ENV,
       preventAssignment: true,
    }),
    postcss({
      plugins: [
        //Fetches any css in @import calls and inlines it
        import_url({ modernBrowser: true }),
        font_grabber({
          fontDest: fonts_dir,
          cssDest: html_dir,
        }),
        autoprefixer(),
      ],
      minimize: true,
    }),
    typescript({
      typescript: require('typescript'),
      include: ['*.js+(|x)', '**/*.js+(|x)'],
      exclude: ['coverage', 'config', 'dist', 'node_modules/**', '*.test.{js+(|x), ts+(|x)}', '**/*.test.{js+(|x), ts+(|x)}'],
    }),
    babel({
      ...pkg.babel,
      extensions: [...DEFAULT_EXTENSIONS, '.ts', '.tsx'],
      babelHelpers: 'runtime',
    }),
    url(),
    svgr(),
    resolve({ browser: true }),
    commonjs(),
    is_prod && terser(),
  ],
};
