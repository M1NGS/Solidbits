<?php

class Solidbits {

    private $_server;
    private $_port;
    private $_socket;
    private $_method = array('bitop', 'bitcount', 'setbit', 'getbit', 'bitcop');
    function __construct($server, $port){
        $this->_server = $server;
        $this->_port = $port;
        $this->_socket = socket_create(AF_INET, SOCK_DGRAM, SOL_UDP);
    }

    private function setTimeout($sec)
    {
        socket_set_option(
            $this->_socket,
            SOL_SOCKET,
            SO_RCVTIMEO,
            array(
              "sec"=>$sec,
              "usec"=>0
            ));
        
    }

    private function doJob($name, $arguments)
    {
        switch ($name) {
            case 'setbit':
                $cmd = 'SETBIT '.$arguments[0].' '.$arguments[1].' '.$arguments[2]."\n";
                $this->setTimeout(3);
                break;
            case 'getbit':
                $buf = '';
                $cmd = 'GETBIT '.$arguments[0].' '.$arguments[1]."\n";
                $this->setTimeout(3);
                break;
            case 'bitcount':
                $cmd = 'BITCOUNT '.$arguments[0];
                isset($arguments[1]) ? $cmd .= ' '.$arguments[1] : NULL;
                isset($arguments[2]) ? $cmd .= ' '.$arguments[2] : NULL;
                $cmd .= "\n";
                $this->setTimeout(60);
                break;
            case 'bitop':
                $cmd = 'BITOP '.$arguments[0].' '.$arguments[1];
                for($i = 2; $i < count($arguments); $i++)
                {
                    $cmd .= ' '.$arguments[$i];
                }
                $cmd .= "\n";
                $this->setTimeout(60);
                break;
            case 'bitcop':
                $cmd = 'BITOP '.$arguments[0].' '.chr(5).'COUNTOP';
                for($i = 1; $i < count($arguments); $i++)
                {
                    $cmd .= ' '.$arguments[$i];
                }
                $cmd .= "\n";
                $this->setTimeout(60);
                break;   
            default:
                return -1;
        }
        socket_sendto($this->_socket, $cmd, strlen($cmd), 0, $this->_server, $this->_port);
        if (socket_recvfrom($this->_socket, $buf, 64, 0, $this->_server, $this->_port) == 0)
        {
            return -1;
        }
        return $buf;
    }
    public function __call ($name , $arguments)
    {
        if (!in_array($name, $this->_method))
        {
            return -1;
        }
        return $this->doJob($name, $arguments);
    }

}
