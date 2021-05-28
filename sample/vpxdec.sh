#!/bin/bash

basedir=$(dirname $0)
../${basedir}/vpxdec --limit=10 --noblit ${basedir}/360p.webm
