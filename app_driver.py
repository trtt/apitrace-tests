#!/usr/bin/env python
##########################################################################
#
# Copyright 2011 Jose Fonseca
# All Rights Reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
##########################################################################/

'''Application test driver.'''


import os.path
import platform
import re
import subprocess
import sys
import time
import json
import base64

try:
    from cStringIO import StringIO
except ImportError:
    from StringIO import StringIO


from base_driver import *


class TraceChecker:

    def __init__(self, srcStream, refFileName, verbose=False):
        self.srcStream = srcStream
        self.refFileName = refFileName
        if refFileName:
            self.refStream = open(refFileName, 'rt')
        else:
            self.refStream = None
        self.verbose = verbose
        self.doubleBuffer = False
        self.callNo = 0
        self.refLine = ''
        self.images = []
        self.states = []

    call_re = re.compile(r'^([0-9]+) (\w+)\(')

    def check(self):

        swapbuffers = 0
        flushes = 0

        srcLines = []
        self.consumeRefLine()
        for line in self.srcStream:
            line = line.rstrip()
            if self.verbose:
                sys.stdout.write(line + '\n')
            mo = self.call_re.match(line)
            if mo:
                self.callNo = int(mo.group(1))
                function_name = mo.group(2)
                if function_name.find('SwapBuffers') != -1 or \
                   line.find('kCGLPFADoubleBuffer') != -1:
                    swapbuffers += 1
                if function_name in ('glFlush', 'glFinish'):
                    flushes += 1
                srcLine = line[mo.start(2):]
            else:
                srcLine = line
            if self.refLine:
                if srcLine == self.refLine:
                    self.consumeRefLine()
                    srcLines = []
                else:
                    srcLines.append(srcLine)

        if self.refLine:
            if srcLines:
                fail('missing call `%s` (found `%s`)' % (self.refLine, srcLines[0]))
            else:
                fail('missing call %s' % self.refLine)

        if swapbuffers:
            self.doubleBuffer = True
        else:
            self.doubleBuffer = False

    def consumeRefLine(self):
        if not self.refStream:
            self.refLine = ''
            return

        while True:
            line = self.refStream.readline()
            if not line:
                break
            line = line.rstrip()
            if line.startswith('#'):
                self.handlePragma(line)
            else:
                break
        self.refLine = line

    def handlePragma(self, line):
        pragma, rest = line.split(None, 1)
        if pragma == '#image':
            imageFileName = self.getAbsPath(rest)
            self.images.append((self.callNo, imageFileName))
        elif pragma == '#state':
            stateFileName = self.getAbsPath(rest)
            self.states.append((self.callNo, stateFileName))
        else:
            assert False

    def getAbsPath(self, path):
        '''Get the absolute from a path relative to the reference filename'''
        return os.path.abspath(os.path.join(os.path.dirname(self.refFileName), path))



class AppDriver(Driver):

    cmd = None
    cwd = None

    api = 'gl'
    max_frames = None
    trace_file = None

    ref_dump = None

    doubleBuffer = True

    verbose = False

    threshold_precision = 12.0

    def __init__(self):
        Driver.__init__(self)
        self.stateCache = {}
    
    def runApp(self):
        '''Run the application standalone, skipping this test if it fails by
        some reason.'''

        if not self.cmd:
            return

        p = popen(self.cmd, cwd=self.cwd)
        p.wait()
        if p.returncode:
            skip('application returned code %i' % p.returncode)

    api_map = {
        'gl': 'gl',
        'egl_gl': 'egl',
        'egl_gles1': 'egl',
        'egl_gles2': 'egl',
    }

    def traceApp(self):
        if not self.cmd:
            return

        if self.trace_file is None:
            if self.ref_dump is not None:
                name = self.ref_dump
            else:
                name = self.cmd[0]
            name, ext = os.path.splitext(os.path.basename(name))
            while ext:
                name, ext = os.path.splitext(os.path.basename(name))
            self.trace_file = os.path.abspath(os.path.join(self.results, name + '.trace'))
        if os.path.exists(self.trace_file):
            os.remove(self.trace_file)
        else:
            trace_dir = os.path.dirname(self.trace_file)
            if not os.path.exists(trace_dir):
                os.makedirs(trace_dir)

        cmd = self.cmd
        env = os.environ.copy()
        
        cmd = [
            options.apitrace, 'trace', 
            '--api', self.api_map[self.api],
            '--output', self.trace_file,
            '--'
        ] + cmd
        if self.max_frames is not None:
            env['TRACE_FRAMES'] = str(self.max_frames)

        p = popen(cmd, env=env, cwd=self.cwd)
        p.wait()

        if not os.path.exists(self.trace_file):
            fail('no trace file generated\n')
    
    def checkTrace(self):
        cmd = [options.apitrace, 'dump', '--color=never', self.trace_file]
        p = popen(cmd, stdout=subprocess.PIPE)

        checker = TraceChecker(p.stdout, self.ref_dump, self.verbose)
        checker.check()
        p.wait()
        if p.returncode != 0:
            fail('`apitrace dump` returned code %i' % p.returncode)

        self.doubleBuffer = checker.doubleBuffer

        for callNo, refImageFileName in checker.images:
            self.checkImage(callNo, refImageFileName)
        for callNo, refStateFileName in checker.states:
            self.checkState(callNo, refStateFileName)

    def checkImage(self, callNo, refImageFileName):
        try:
            from PIL import Image
        except ImportError:
            return

        srcImage = self.getImage(callNo)
        refImage = Image.open(refImageFileName)

        from snapdiff import Comparer
        comparer = Comparer(refImage, srcImage)
        precision = comparer.precision(filter=True)
        sys.stdout.write('precision of %f bits against %s\n' % (precision, refImageFileName))
        if precision < self.threshold_precision:
            prefix = '%s.%u' % (self.getNamePrefix(), callNo)
            srcImageFileName = prefix + '.src.png'
            srcImage.save(srcImageFileName)
            diffImageFileName = prefix + '.diff.png'
            comparer.write_diff(diffImageFileName)
            fail('snapshot from call %u does not match %s' % (callNo, refImageFileName))

    def checkState(self, callNo, refStateFileName):
        srcState = self.getState(callNo)
        refState = self.getRefState(refStateFileName)

        from jsondiff import Comparer, Differ
        comparer = Comparer(ignore_added = True)
        match = comparer.visit(refState, srcState)
        if not match:
            prefix = '%s.%u' % (self.getNamePrefix(), callNo)
            srcStateFileName = prefix + '.src.json'
            diffStateFileName = prefix + '.diff.json'
            self.saveState(srcState, srcStateFileName)
            #diffStateFile = open(diffStateFileName, 'wt')
            diffStateFile = sys.stdout
            differ = Differ(diffStateFile, ignore_added = True)
            differ.visit(refState, srcState)
            fail('state from call %u does not match %s' % (callNo, refStateFileName))

    def getRefState(self, refStateFileName):
        stream = open(refStateFileName, 'rt')
        from jsondiff import load
        state = load(stream)
        self.adjustRefState(state)
        return state

    def getNamePrefix(self):
        name = os.path.basename(self.ref_dump)
        try:
            index = name.index('.')
        except ValueError:
            pass
        else:
            name = name[:index]
        return name

    def saveState(self, state, filename):
        s = json.dumps(state, sort_keys=True, indent=2)
        open(filename, 'wt').write(s)

    def retrace(self):
        p = self._retrace()
        p.wait()
        if p.returncode != 0:
            fail('retrace failed with code %i' % (p.returncode))

    def getImage(self, callNo):
        from PIL import Image
        state = self.getState(callNo)
        if self.doubleBuffer:
            attachments = ['GL_BACK', 'GL_BACK_LEFT', 'GL_BACK_RIGHT', 'GL_COLOR_ATTACHMENT0']
        else:
            attachments = ['GL_FRONT', 'GL_FRONT_LEFT', 'GL_FRONT_RIGHT', 'GL_COLOR_ATTACHMENT0']
        imageObj = self.getFramebufferAttachment(state, attachments)
        data = imageObj['__data__']
        stream = StringIO(base64.b64decode(data))
        im = Image.open(stream)
        im.save('test.png')
        return im

    def getFramebufferAttachment(self, state, attachments):
        framebufferObj = state['framebuffer']
        for attachment in attachments:
            try:
                attachmentObj = framebufferObj[attachment]
            except KeyError:
                pass
            else:
                return attachmentObj
        raise Exception("no attachment found")

    def getState(self, callNo):
        try:
            state = self.stateCache[callNo]
        except KeyError:
            pass
        else:
            return state

        p = self._retrace(['-D', str(callNo)])
        state = json.load(p.stdout, strict=False)
        p.wait()
        if p.returncode != 0:
            fail('retrace returned code %i' % (p.returncode))

        self.adjustSrcState(state)

        self.stateCache[callNo] = state

        return state

    def adjustSrcState(self, state):
        # Do some adjustments on the obtained state to eliminate failures from
        # bugs/issues outside of apitrace

        try:
            parameters = state['parameters']
        except KeyError:
            return

        # On NVIDIA drivers glGetIntegerv(GL_INDEX_WRITEMASK) returns -1
        self.replaceState(parameters, 'GL_INDEX_WRITEMASK', 255, -1)

        # On Gallium 
        if 'Gallium' in parameters['GL_RENDERER'].split():
            # Gallium drivers have wrong defaults for draw/read buffer state
            self.replaceState(parameters, 'GL_DRAW_BUFFER', 'GL_BACK_LEFT', 'GL_BACK')
            self.replaceState(parameters, 'GL_DRAW_BUFFER0', 'GL_BACK_LEFT', 'GL_BACK')
            self.replaceState(parameters, 'GL_READ_BUFFER', 'GL_BACK_LEFT', 'GL_BACK')
            self.replaceState(parameters, 'GL_DRAW_BUFFER', 'GL_FRONT_LEFT', 'GL_FRONT')
            self.replaceState(parameters, 'GL_DRAW_BUFFER0', 'GL_FRONT_LEFT', 'GL_FRONT')
            self.replaceState(parameters, 'GL_READ_BUFFER', 'GL_FRONT_LEFT', 'GL_FRONT')

    def adjustRefState(self, state):
        # Do some adjustments on reference state to eliminate failures from
        # bugs/issues outside of apitrace

        try:
            parameters = state['parameters']
        except KeyError:
            return

        if platform.system() == 'Darwin':
            # Mac OS X drivers fail on GL_COLOR_SUM
            # XXX: investigate this
            self.removeState(parameters, 'GL_COLOR_SUM')

    def replaceState(self, obj, key, srcValue, dstValue):
        try:
            value = obj[key]
        except KeyError:
            pass
        else:
            if value == srcValue:
                obj[key] = dstValue

    def removeState(self, obj, key):
        try:
            del obj[key]
        except KeyError:
            pass

    def _retrace(self, args = None, stdout=subprocess.PIPE):
        retrace = self.api_map[self.api] + 'retrace'
        cmd = [get_build_program(retrace)]
        if self.doubleBuffer:
            cmd += ['-db']
        else:
            cmd += ['-sb']
        if args:
            cmd += args
        cmd += [self.trace_file]
        return popen(cmd, stdout=stdout)

    def createOptParser(self):
        optparser = Driver.createOptParser(self)

        optparser.add_option(
            '-a', '--api', metavar='API',
            type='string', dest='api', default='gl',
            help='api to trace')
        optparser.add_option(
            '-R', '--results', metavar='PATH',
            type='string', dest='results', default='.',
            help='results directory [default=%default]')
        optparser.add_option(
            '--ref-dump', metavar='PATH',
            type='string', dest='ref_dump', default=None,
            help='reference dump')

        return optparser

    def run(self):
        global options

        (options, args) = self.parseOptions()

        if not os.path.exists(options.results):
            os.makedirs(options.results)

        self.verbose = options.verbose

        self.cmd = args
        self.cwd = options.cwd
        self.api = options.api
        self.ref_dump = options.ref_dump
        self.results = options.results

        self.runApp()
        self.traceApp()
        self.checkTrace()
        self.retrace()

        pass_()

if __name__ == '__main__':
    AppDriver().run()