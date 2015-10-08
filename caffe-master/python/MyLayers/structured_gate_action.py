import unittest
import tempfile
import os
import numpy

import caffe

class structured_gate(caffe.Layer):
    """A layer that initialize messages for recurrent belief propagation"""

    def setup(self, bottom, top):
        self.nScene = 5
        self.nAction = 40
        self.nPeople = 14
        self.K_ = 0;
        self.bottom_batchsize = 0
        self.slen = 0
        self.alen = 0
        self.tlen_leaf = 0
        self.tlen_mid = 0
        self.sunit = 0
        self.aunit = 0
        self.tunit = 0
        self.message_num_action = self.nPeople+1+2*(self.K_>0)
        self.label_stop = []
        self.top_batchsize = 0
        self.on_edge = True
        self.zero2one = True
        self.lamda = 5
    
    def reshape(self, bottom, top):
        # have 3 inputs: gate, a2a message, labels
        # have one output: gated a2a message 
        bottom_batchsize = bottom[2].data.shape[0]
        edge_num = self.nPeople
        self.bottom_batchsize = bottom_batchsize/self.nPeople
        top[0].reshape(*bottom[1].data.shape)

    def forward(self, bottom, top):
        gate_input = bottom[0].data.copy()
        messages = bottom[1].data.copy()
        label_stop = self.nPeople*numpy.ones([self.bottom_batchsize])
        labels = bottom[2].data
        count = 0
        for i in range(0,self.bottom_batchsize):
            for j in range(0,self.nPeople):
                if labels[i*self.nPeople+j] == 0:
                    label_stop[i] = j
                    break
        self.label_stop = label_stop
        # the paired up inputs should be:
        # [(1,2),(2,1)]   [(1,3),(3,1)]   [(1,4),(4,1)]   [(1,5),(5,1)]   [(1,6),(6,1)]
        # [(2,3),(3,2)]   [(2,4),(4,2)]   [(2,5),(5,2)]   [(2,6),(6,2)]
        # [(3,4),(4,3)]   [(3,5),(5,3)]   [(3,6),(6,3)]
        # [(4,5),(5,4)]   [(4,6),(6,4)]
        # [(5,6),(6,5)]
        # gate design:
        s_gate = numpy.zeros([self.nPeople*self.bottom_batchsize,self.nPeople*self.bottom_batchsize])
        
        zero2one = self.zero2one
        count = 0
        step = 0
        for f in range(0,self.bottom_batchsize):
            step = f*self.nPeople
            for i in range(0,self.nPeople):
                for j in range(0,self.nPeople):
                    if i==j or i >= label_stop[f] or j >= label_stop[f]:
                        continue
                    s_gate[step+i,step+j] += gate_input[count]
                    s_gate[step+j,step+i] += gate_input[count]
                    count += 1
        count = 0
        for f in range(0,self.bottom_batchsize):
            step = f*self.nPeople
            for i in range(0,self.nPeople):
                for j in range(0,self.nPeople):
                    if i == j or i >= label_stop[f] or j >= label_stop[f]:
                        continue
                    o = (1+numpy.tanh(s_gate[step+i,step+j]))/2.0
                    messages[count] = numpy.multiply(o,messages[count])
        top[0].data[...] = messages

    def backward(self, top, propagate_down, bottom):
        # to be written
        # diffs for : bottom[0] -> paired gates input; bottom[1] -> messages
        # diffs from top: gated_messages
        gate_input = bottom[0].data.copy()
        gates_diff = bottom[0].diff.copy()
        messages = bottom[1].data
        #message_diff = bottom[1].diff.copy()
        gated_message_diff = top[0].diff.copy()

        label_stop = self.nPeople*numpy.ones([self.bottom_batchsize])
        labels = bottom[2].data
        count = 0
        for i in range(0,self.bottom_batchsize):
            for j in range(0,self.nPeople):
                if labels[i*self.nPeople+j] == 0:
                    label_stop[i] = j
                    break
        self.label_stop = label_stop

        # gates diff:
        # the paired up inputs should be:
        # [(1,2),(2,1)]   [(1,3),(3,1)]   [(1,4),(4,1)]   [(1,5),(5,1)]   [(1,6),(6,1)]
        # [(2,3),(3,2)]   [(2,4),(4,2)]   [(2,5),(5,2)]   [(2,6),(6,2)]
        # [(3,4),(4,3)]   [(3,5),(5,3)]   [(3,6),(6,3)]
        # [(4,5),(5,4)]   [(4,6),(6,4)]
        # [(5,6),(6,5)]

        # gate design:
        on_edge = self.on_edge
        s_gate = numpy.zeros([self.nPeople*self.bottom_batchsize,self.nPeople*self.bottom_batchsize])
       
        # non-linearity design:
        zero2one = self.zero2one
        count = 0
        step = 0
        for f in range(0,self.bottom_batchsize):
            step = f*self.nPeople
            for i in range(0,self.nPeople):
                for j in range(0,self.nPeople):
                    if i==j or i >= label_stop[f] or j >= label_stop[f]:
                        continue
                    s_gate[step+i,step+j] += gate_input[count]
                    s_gate[step+j,step+i] += gate_input[count]
                    count += 1
        count = 0
        idx = 0
        for f in range(0,self.bottom_batchsize):
            step = f*self.nPeople
            for i in range(0,self.nPeople):
                for j in range(0,self.nPeople):
                    if i == j or i >= label_stop[f] or j >= label_stop[f]:
                        continue
                    diff = numpy.multiply(gated_message_diff[count],messages[count])
                    o = (1+numpy.tanh(s_gate[step+i,step+j]))/2.0
                    tanh_sq = numpy.multiply(o,o)
                    gates_diff[count] = numpy.sum(diff)*(1-tanh_sq)
                    count += 1
                    #print o 
        gate_input = bottom[0].data.copy()
        gates_diff += self.lamda*gate_input
        bottom[0].diff[...] = gates_diff
        #bottom[1].diff[...] = message_diff
        
        

def python_net_file():
    with tempfile.NamedTemporaryFile(delete=False) as f:
        f.write("""name: 'pythonnet' force_backward: true
        input: 'data' input_shape { dim: 10 dim: 9 dim: 8 }
        layer { type: 'Python' name: 'one' bottom: 'data' top: 'one'
          python_param { module: 'test_python_layer' layer: 'SimpleLayer' } }
        layer { type: 'Python' name: 'two' bottom: 'one' top: 'two'
          python_param { module: 'test_python_layer' layer: 'SimpleLayer' } }
        layer { type: 'Python' name: 'three' bottom: 'two' top: 'three'
          python_param { module: 'test_python_layer' layer: 'SimpleLayer' } }""")
        return f.name

class TestPythonLayer(unittest.TestCase):
    def setUp(self):
        net_file = python_net_file()
        self.net = caffe.Net(net_file, caffe.TRAIN)
        os.remove(net_file)

    def test_forward(self):
        x = 8
        self.net.blobs['data'].data[...] = x
        self.net.forward()
        for y in self.net.blobs['three'].data.flat:
            self.assertEqual(y, 10**3 * x)

    def test_backward(self):
        x = 7
        self.net.blobs['three'].diff[...] = x
        self.net.backward()
        for y in self.net.blobs['data'].diff.flat:
            self.assertEqual(y, 10**3 * x)

    def test_reshape(self):
        s = 4
        self.net.blobs['data'].reshape(s, s, s, s)
        self.net.forward()
        for blob in self.net.blobs.itervalues():
            for d in blob.data.shape:
                self.assertEqual(s, d)
