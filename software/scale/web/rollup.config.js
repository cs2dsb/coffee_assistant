import { DEFAULT_EXTENSIONS } from '@babel/core';
import babel from '@rollup/plugin-babel';
import typescript from 'rollup-plugin-typescript2';
import commonjs from '@rollup/plugin-commonjs';
import postcss from 'rollup-plugin-postcss';
import resolve from '@rollup/plugin-node-resolve';
import url from '@rollup/plugin-url';
import svgr from '@svgr/rollup';
import { terser } from 'rollup-plugin-terser';
import replace from '@rollup/plugin-replace';
import eslint from '@rollup/plugin-eslint';
import copy from 'rollup-plugin-copy'
import pkg from './package.json';

const is_prod = process.env.NODE_ENV == "production";
const NODE_ENV = is_prod ? "\'production\'" : "\'development\'";

export default {
  input: ['src/js/index.tsx'],
  output: [
    {
      sourcemap: true,
      dir: 'dist/js',
      format: 'esm',
    },
  ],
  plugins: [
    eslint(),
    copy({
      targets: [
        { src: 'src/favicon.ico', dest: 'dist' },
        { src: 'src/html/*.html', dest: 'dist/html' },
    ]}),
    replace({
       "process.env.NODE_ENV": NODE_ENV,
       preventAssignment: true,
    }),
    postcss({
      plugins: [],
      minimize: true,
    }),
    // external({
    //   includeDependencies: true,
    // }),
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
    resolve({
      browser: true,
      //modulesOnly: true,
      //Force deduping these. Dupes break react.
      //dedupe: [ 'react', 'react-dom' ],
      // moduleDirectories: [
      //   'src',
      // ],
    }),
    commonjs(),
    is_prod && terser(),
  ],
};
