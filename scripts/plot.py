#!/usr/bin/env python3

import sqlite3
import sys
import os
from matplotlib import pyplot

os.chdir(os.path.dirname(os.path.dirname(os.path.realpath(__file__))))

db = sqlite3.connect('measurements.sqlite')

# COMMIT = os.popen("git rev-parse HEAD").read()[:-1]

MEAN = 'D'
POINT = '.'
SCATTER = '+'

T = 'TAG'
C = 'CONFIGURATION'

FEATURES = ('name', 'variables', 'clauses', 'max_clause_length', 'time',
            'clauses_preprocessed', 'removed_clauses_average_length',
            'l_can_be_removed', 'l_cannot_be_removed', 'number_of_eliminated_clauses')

# feature index
FI = {}
for feature in FEATURES:
    FI[feature] = len(FI)

FPP = ['formula', 'variables', 'clauses', 'maximum clause length', 'preprocessing time',
       'clauses after preprocessing', 'average size of removed clauses', 'l can be removed from literal_pool', 'l cannot be removed', 'number of eliminated clauses']


def fetch(configuration):
    result = db.execute("""
select
f.name, f.variables, f.clauses, f.max_clause_length, p.time,
p.clauses_preprocessed, p.removed_clauses_average_length, p.l_can_be_removed,
p.l_cannot_be_removed, f.clauses - p.clauses_preprocessed

from formula f, preprocessing p
where f.name = p.name and p.configuration = "%s"
order by f.name
""" % configuration)

    return [row for row in result]

def plot(configs, yfeatures, x='', sorting='time'):
    _plot(configs, x, yfeatures, pyplot.plot, sorting)

def plog(configs, yfeatures, x='', sorting='time'):
    _plot(configs, x, yfeatures, pyplot.semilogy, sorting)

DIR = 'plots'
if not os.path.exists(DIR):
    os.mkdir(DIR)

def _plot(configs, xfeature, yfeatures, plotfun, sorting):
    if sorting is not None:
        config = configs[0]
        data = fetch(config[C])
        si = FI[sorting]
        sl = []
        for row in data:
            sl += [(row[si], len(sl))]
        indices = [s[1] for s in sorted(sl)]
    for config in configs:
        data = fetch(config[C])
        if sorting is not None:
            config['data'] = [data[indices[i]] for i in range(len(data))]
        else:
            config['data'] = data
    cs = ''
    for config in configs:
        cs += '-' + config[C].replace(' ', '+')
    filename = '%s/c%s.png' % (DIR, cs)
    for yfeature in yfeatures:
        for config in configs:
            data = config['data']
            def f(feature):
                if not feature:
                    return range(len(data))
                return [row[FI[feature]] for row in data]
            label = config[T] if len(yfeatures) == 1 else FPP[FI[yfeature]]
            plotfun(f(xfeature), f(yfeature), (SCATTER if xfeature else POINT), label=label)
    if len(yfeatures) == 1:
        pyplot.ylabel(FPP[FI[yfeature]])
    if xfeature:
        pyplot.xlabel(FPP[FI[xfeature]])
    else:
        pyplot.gca().axes.get_xaxis().set_ticks([])
    if label:
        pyplot.legend(bbox_to_anchor=(1.0, -0.1))
    pyplot.savefig(filename, bbox_inches='tight')
    pyplot.close()

def plot_solving_time():
    result = db.execute('select f.original_solving_time_lingeling, s.preprocessed_solving_time_lingeling, p.time from formula f, solving_time s, preprocessing p where f.name = s.name and f.name = p.name and p.configuration = "" and round(f.original_solving_time_lingeling) != round(s.preprocessed_solving_time_lingeling);')
    data = [row for row in result]
    data.sort(key=lambda row: row[0])
    x = [3 * i for i in range(len(data))]
    pyplot.gca().axes.get_xaxis().set_ticks([])
    orig = [row[0] for row in data]
    prep = [row[1] for row in data]
    time = [row[2] for row in data]
    width = 1.0
    def offset(o):
        return [o + _ for _ in x]
    pyplot.bar(offset(0), orig, width, color='g', label='original solving time')
    pyplot.bar(offset(width), prep, width, color='y', label='solving time after SPR elimination')
    pyplot.bar(offset(width), time, width, color='b', label='preprocessing time', bottom=prep)
    pyplot.ylabel('time')
    pyplot.legend(bbox_to_anchor=(1.0, -0.1))
    pyplot.savefig('%s/solving_time.png' % DIR, bbox_inches='tight')
    pyplot.close()

def plot_removed_clauses_difference():
    result = db.execute('select rat.clauses_preprocessed - spr.clauses_preprocessed from formula f, preprocessing rat, preprocessing spr where f.name = rat.name and f.name = spr.name and rat.configuration="PROPERTY:=RAT" and spr.configuration="";')
    data = [row for row in result]
    data.sort(key=lambda row: row[0])
    x = [i for i in range(len(data))]
    pyplot.gca().axes.get_xaxis().set_ticks([])
    y = [row[0] for row in data]
    pyplot.plot(x, y, POINT)
    pyplot.ylabel('difference in eliminated clauses')
    # pyplot.legend(bbox_to_anchor=(1.0, -0.1))
    pyplot.savefig('%s/removed_clauses_difference.png' % DIR, bbox_inches='tight')
    pyplot.close()

def plot_removed_clauses():
    result = db.execute('select f.clauses, f.clauses - spr.clauses_preprocessed from formula f, preprocessing spr where f.name = spr.name and spr.configuration="" order by f.clauses;')
    data = [row for row in result]
    data.sort(key=lambda row: row[0])
    # x = [i for i in range(len(data))]
    # pyplot.gca().axes.get_xaxis().set_ticks([])
    x = [row[0] for row in data]
    y = [row[1] for row in data]
    pyplot.plot(x, y, SCATTER)
    pyplot.ylabel('eliminated clauses')
    # pyplot.legend(bbox_to_anchor=(1.0, -0.1))
    pyplot.savefig('%s/removed_clauses.png' % DIR, bbox_inches='tight')
    pyplot.close()

def plot_L_sizes():
    import numpy as np
    result = db.execute('select s1, s2, s3, s4 from L_sizes order by s1;')
    data = [row for row in result]
    x = [i for i in range(len(data))]
    pyplot.gca().axes.get_xaxis().set_ticks([])
    s1 = [row[0] for row in data]
    s2 = [row[1] for row in data]
    s3 = [row[2] for row in data]
    s4 = [row[3] for row in data]
    width = 0.5
    pyplot.bar(x, s1, width, color='g')
    pyplot.savefig('%s/L_size_1.png' % DIR, bbox_inches='tight')
    pyplot.close()
    pyplot.bar(x, s2, width, color='b', label='size 2')
    pyplot.bar(x, s3, width, color='r', label='size 3', bottom=s2)
    pyplot.bar(x, s4, width, color='y', label='size 4', bottom=s3)
    pyplot.legend(bbox_to_anchor=(1.0, -0.1))
    pyplot.savefig('%s/L_size_2_through_4.png' % DIR, bbox_inches='tight')
    pyplot.close()

baseline = {T:'baseline',C:''}

plog([baseline, {T:'RAT elimination only',C:'PROPERTY:=RAT'}], ['time'])
plog([baseline, {T:'with watched literals',C:'WATCH_LITERALS:=true'}], ['time'])
plog([baseline, {T:'no occurrence list sorting',C:'SORT_VARIABLE_OCCURRENCES:=false'}], ['time'])
plog([baseline, {T:'no refutation caching',C:'STORE_REFUTATIONS:=false'}], ['time'])
plog([baseline, {T:'no detection of environment change',C:'DETECT_ENVIRONMENT_CHANGE:=false'}], ['time'])
plog([baseline, {T:'no small clause optimization',C:'SMALL_CLAUSE_OPTIMIZATION:=false'}], ['time'])
plog([baseline,
      {T:'parallel (pthreads, 3 worker threads)',C:'PARALLEL:=PTHREADS THREADS:=3'},
      {T:'parallel (pthreads, 4 worker threads)',C:'PARALLEL:=PTHREADS THREADS:=4'},
      {T:'parallel (lace)',C:'PARALLEL:=LACE'}
      ], ['time'])
plot([{T:'',C:''}], ['removed_clauses_average_length'], x='clauses')

plot_solving_time()
plot_removed_clauses()
plot_removed_clauses_difference()
plot_L_sizes()
