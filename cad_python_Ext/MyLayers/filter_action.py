import unittest
import tempfile
import os
import numpy

import caffe

class filter_action(caffe.Layer):
    """A layer that initialize messages for recurrent belief propagation"""

    def setup(self, bottom, top):
        self.nScene = 5
        self.nAction = 40
        self.nPeople = 14
        self.K_ = 0;
        self.bottom_batchsize = 0
        self.unitlen = 0
        self.output_num = 0
        self.label_stop = []
    
    def reshape(self, bottom, top):
        # have one input one output, initialize messages for each node in the graphical model 
        bottom_shape = bottom[0].data.shape
        bottom_batchsize = bottom_shape[0]
        top[0].reshape(bottom_batchsize, bottom_shape[1]*bottom_shape[2])
        self.bottom_batchsize = bottom_batchsize
        self.nPeople = bottom_shape[1]
        self.unitlen = bottom_shape[2]
        self.output_num = self.unitlen*self.nPeople
        #print self.output_num

    def forward(self, bottom, top):
        label_stop = self.nPeople*numpy.ones([self.bottom_batchsize])
        labels = bottom[1].data
        count = 0
        for i in range(0,self.bottom_batchsize):
            for j in range(0,self.nPeople):
                if labels[i*self.nPeople+j] == 0:
                    label_stop[i] = j
                    break
        self.label_stop = label_stop
        tmpdata = numpy.reshape(bottom[0].data,[self.bottom_batchsize,self.output_num])
        for i in range(0,self.bottom_batchsize):
            tmpdata[i,self.unitlen*self.label_stop[i]:self.output_num] = numpy.zeros([1,(self.nPeople-self.label_stop[i])*self.unitlen])
            top[0].data[i] = tmpdata[i]
        #print top[0].data[0]
        #print self.label_stop[0]

    def backward(self, top, propagate_down, bottom):
        pass

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
