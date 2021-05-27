#!/bin/bash

basedir=$(dirname $0)
../${basedir}/vpxdec --limit=10 --noblit --progress ${basedir}/360p.webm
