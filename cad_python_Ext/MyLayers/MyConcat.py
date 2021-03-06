import unittest
import tempfile
import os
import numpy

import caffe

class MyConcat(caffe.Layer):
    """A layer that initialize messages for recurrent belief propagation"""

    def setup(self, bottom, top):
        pass
    
    def reshape(self, bottom, top):
        bottom_shape = list(bottom[0].data.shape)
        top_shape = bottom_shape
        top_shape[0] = len(bottom)*top_shape[0]
        top[0].reshape(*top_shape)

    def forward(self, bottom, top):
        tmpdata = bottom[0].data
        for i in range(1,len(bottom)):
            tmpdata = numpy.append(tmpdata,bottom[i].data,axis = 0)
        top[0].data[...] = tmpdata
        #if len(bottom[1].data[0]) == 5:
        #    print bottom[1].data
        
    def backward(self, top, propagate_down, bottom):
        step = len(top[0].data)/len(bottom)
        #print top[0].diff
        for i in range(0,len(bottom)):
            bottom[i].diff[...] = top[0].diff[i*step:(i+1)*step]
        #print 'bottom1:',bottom[0].diff
        #print 'bottom2:',bottom[1].diff
        
        

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
