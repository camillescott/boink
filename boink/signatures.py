#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# (c) Camille Scott, 2019
# File   : signatures.py
# License: MIT
# Author : Camille Scott <camille.scott.w@gmail.com>
# Date   : 15.10.2019

import blessings
import curio
import numpy as np
from pyfiglet import Figlet

from boink import libboink, __version__
from boink.cli import CommandRunner, get_output_interval_args
from boink.parsing import get_pairing_args, iter_fastx_inputs
from boink.processors import AsyncSequenceProcessor
from boink.messages import Interval, DistanceCalc, SampleStarted, SampleFinished
from boink.utils import Namespace

SourmashSignature = libboink.signatures.SourmashSignature
UnikmerSignature  = libboink.signatures.UnikmerSignature


class SourmashRunner(CommandRunner):

    def __init__(self, parser):
        get_output_interval_args(parser)
        group = get_pairing_args(parser)
        group.add_argument('-i', dest='inputs', nargs='+', required=True)
        parser.add_argument('-K', default=31, type=int)
        parser.add_argument('-N', default=10000, type=int)
        parser.add_argument('--echo', default=False, action='store_true',
                            help='echo all events to the terminal.')
        parser.add_argument('--term-graph', default=False, action='store_true',
                            help='draw a live distance graph the terminal.')

        parser.add_argument('--distance-output', nargs='?')
        parser.add_argument('--stdev-cutoff', default=0.00001, type=float)
        parser.add_argument('--window-size', default=5, type=int)


        super().__init__(parser)

    def postprocess_args(self, args):
        if args.term_graph:
            self.term_graph = Namespace()
            self.term_graph.term = blessings.Terminal()
            # minghash sigs only use jaccard distance
            args.distance_metric = 'jaccard'

    def setup(self, args):
        # build the sourmash signature
        self.signature = SourmashSignature.Signature.build(args.N, args.K, False, 42, 0)

        # build the underlying Processor specialized for sourmash signature
        processor = SourmashSignature.Processor.build(self.signature,
                                                      args.fine_interval,
                                                      args.medium_interval,
                                                      args.coarse_interval)

        # get the sample iter
        sample_iter = iter_fastx_inputs(args.inputs, args.pairing_mode)

        # build and save the async sequence processor
        self.processor = AsyncSequenceProcessor(processor, sample_iter, args.echo)
        
        # set up the saturation tracker
        def dfunc(sig_a, sig_b):
            return sig_a.similarity(sig_b)
        self.tracker = SaturationTracker(args.window_size, args.stdev_cutoff, dfunc)

        # set up a callback from Interval events on the sequence processor
        def on_interval(msg, events_q):
            sig = self.signature.to_sourmash()
            distance, delta, stdev = self.tracker.push(sig, msg.t)
            if distance is not None:
                out_msg = DistanceCalc(sample_name=msg.sample_name,
                                       t=msg.t,
                                       delta=delta,
                                       distance=distance,
                                       stdev=stdev)
                # note events_q is a UniversalQueue so doesn't need to be awaited
                events_q.put(out_msg)
        
        # set up the listener and add the callback
        self.interval_listener = self.processor.add_listener('worker_q', 'distances')
        self.interval_listener.on_message(Interval, on_interval,
                                          self.processor.events_q)
        
        if args.term_graph:
            self.init_term_graph(args)
    
    def execute(self, args):
        if args.term_graph:
            with self.term_graph.term.hidden_cursor():
                curio.run(self.processor.run, with_monitor=True)
        else:
            curio.run(self.processor.run, with_monitor=True)

    def teardown(self):
        pass

    def init_term_graph(self, args):
        self.term_graph.frame = SignatureStreamFrame(self.term_graph.term, args)
        frame = self.term_graph.frame

        # set up frame drawing callbacks
        async def on_samplestart(msg):
            draw = await curio.spawn(frame.draw, messages=[f'Started processing on {msg.samplename}'], draw_dist_plot=False)
            await draw.join()
        self.term_graph.ss_listener = self.processor.add_listener('events_q', 'framedraw.samplestart')
        self.term_graph.ss_listener.on_message(SampleStarted, on_samplestart)
        
        async def on_distancecalc(msg):
            draw = await curio.spawn(frame.draw, msg.t, msg.distance, msg.stdev)
            await draw.join()
        self.term_graph.dc_listener = self.processor.add_listener('events_q', 'framedraw.distancecalc')
        self.term_graph.dc_listener.on_message(DistanceCalc, on_distancecalc)
        

class SaturationTracker:

    def __init__(self, window_size, stdev_cutoff, dfunc):
        self.window_size = window_size
        self.stdev_cutoff = stdev_cutoff

        self.saturated  = False
        self.signatures = []
        self.distances  = []
        self.times      = []
        self.stdevs     = []
        self.dfunc      = dfunc
        self.prev_sig   = None

    def push(self, new_sig, new_time):
        if self.prev_sig:
            self.distances.append(self.dfunc(self.prev_sig, new_sig))
            self.times.append(new_time)
            retval = self.distances[-1], self.times[-1], np.NaN
        else:
            retval = None, None, None
        self.prev_sig = new_sig

        if len(self.distances) >= self.window_size:
            stdev = np.std(self.distances[-self.window_size:])
            self.stdevs.append(stdev)
            retval = self.distances[-1], self.times[-1], self.stdevs[-1]

            if len(self.stdevs) >= self.window_size and \
                    all((d < self.stdev_cutoff for d in self.stdevs[-self.window_size:])):
                self.saturated = True                

        return retval


class TextBlock:

    def __init__(self, text):
        self.text = text.split('\n')
        self.height = len(self.text)
        self.width = max((len(line) for line in self.text))

    def __iter__(self):
        yield from self.text

    def draw(self, term, x, shift_y=None):
        out = term.stream
        
        if shift_y:
            buf = term.move_down * shift_y
        else:
            buf = ''

        for line in self.text:
            buf += term.move_x(x)
            buf += line
            buf += term.move_down
        
        out.write(buf)


class SignatureStreamFrame:

    def __init__(self, term, args, component_name='sourmash stream'):
        import plotille as pt
        self.term = term
        self.args   = args

        self.distance_figure = pt.Figure()
        self.distance_figure.width = 60
        self.distance_figure.height = 20
        self.distance_figure.x_label = '# sequences'
        self.distance_figure.y_label = 'Δdistance'
        self.distance_figure.set_x_limits(min_=0)
        self.distance_figure.set_y_limits(min_=0)
        self.distance_figure.color_mode = 'byte'
        self.distance_figure.origin = False

        self.hist_figure = pt.Figure()
        self.hist_figure.width = 30
        self.hist_figure.height = 20
        self.hist_figure.x_label = 'Δdistance'
        self.hist_figure.y_label = 'counts'
        self.hist_figure.color_mode = 'byte'

        name_block = Figlet(font='doom').renderText('boink').rstrip().split('\n')
        name_block[-1] = name_block[-1] + f' {__version__}'
        name_block.append(f'\n{term.bold}{component_name}{term.normal}')
        self.name_block = TextBlock('\n'.join(name_block))

        param_block = term.normal + term.underline + (' ' * 40) + term.normal + '\n\n'
        param_block += '{term.bold}distance window size:  {term.normal}{distance_window}\n'\
                       '{term.bold}distance metric:       {term.normal}{distance_metric}\n'\
                       '{term.bold}distance stdev cutoff: {term.normal}{stdev_cutoff}'.format(term = self.term,
                                                                                              distance_window = args.window_size,
                                                                                              distance_metric = args.distance_metric,
                                                                                              stdev_cutoff    = args.stdev_cutoff)
        self.param_block = TextBlock(param_block)
        metric_text = term.normal + term.underline + (' ' * 40) + term.normal + '\n'
        metric_text += '{term.move_down}{term.bold}'
        metric_text += '# sequences:  '.ljust(20)
        metric_text += '{term.normal}{n_reads:,}\n'
        metric_text += '{term.bold}'
        metric_text += 'Δdistance:    '.ljust(20)
        metric_text += '{term.normal}{prev_d:.20f}\n'
        metric_text += '{term.bold}'
        metric_text += 'σ(Δdistance): '.ljust(20)
        metric_text += '{term.normal}{stdev:.20f}\n'
        metric_text += '{term.underline}' + (' ' * 40) + '{term.normal}'
        self.metric_text = metric_text

        #message_text = term.normal + term.underline + (' ' * 40) + term.normal + '\n'
        message_text = '{messages}'
        self.message_text = message_text

        self.distances = []
        self.distances_t = []

    def __del__(self):
        self._print(self.term.move_down * (self.distance_figure.height + 10))

    def _print(self, *args, **kwargs):
        self.term.stream.write(*args, **kwargs)

    @curio.async_thread
    def metric_block(self, n_reads, prev_d, stdev):
        return TextBlock(self.metric_text.format(n_reads=n_reads, prev_d=prev_d, stdev=stdev, term=self.term))

    def message_block(self, messages):
        text = self.message_text.format(messages='\n'.join(messages))
        return TextBlock(text)

    @curio.async_thread
    def figure_block(self, distances, distances_t):
        self.distance_figure.clear()
        self.distance_figure.plot(distances_t, distances, lc=10, interp=None)
        text = self.distance_figure.show()
        return TextBlock(text)

    def hist_block(self, distances):
        self.hist_figure.clear()
        distances = np.log(distances)
        self.hist_figure.histogram(distances, lc=20)
        text = self.hist_figure.show()
        return TextBlock(text)

    async def draw(self, n_reads=0, prev_d=1.0, stdev=1.0,
                   messages=None, draw_dist_plot=True,
                   draw_dist_hist=False):

        term = self.term
        self.distances.append(prev_d)
        self.distances_t.append(n_reads)

        async with curio.TaskGroup() as g:
            figure = await self.figure_block(self.distances, self.distances_t)
            metrics = await self.metric_block(n_reads, prev_d, stdev)
        
        term.stream.write(term.clear_eos)

        if draw_dist_plot:
            with term.location():
                figure.draw(term, max(self.name_block.width, self.param_block.width) + 1, 1)

        with term.location():
            self.name_block.draw(term, 0, shift_y=1)

        with term.location():
            self.param_block.draw(term, 0, shift_y=self.name_block.height+4)

        with term.location():
            metrics.draw(term,
                            0,
                            self.name_block.height + self.param_block.height + 4)

        if messages is not None:
            with term.location():
                ypos = _metric_block.height + self.name_block.height + self.param_block.height + 9
                self.message_block(messages).draw(term, 0, ypos)

        if draw_dist_hist and distances is not None:
            with term.location():
                xpos = max(self.name_block.width, self.param_block.width) + 2
                if draw_dist_plot:
                    xpos += self.distance_figure.width + 5
                self.hist_block(distances).draw(term, xpos, 1)