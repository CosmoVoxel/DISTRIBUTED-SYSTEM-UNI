Vagrant.configure("2") do |config|
  config.vm.box = "ubuntu/focal64"
  #config.vm.synced_folder "C:/Users/marce/PycharmProjects/pythonProject17"

  config.vm.define "node1" do |node1|
    node1.vm.hostname = "node1"
    node1.vm.network "private_network", ip: "192.168.56.101"
  end

  config.vm.define "node2" do |node2|
    node2.vm.hostname = "node2"
    node2.vm.network "private_network", ip: "192.168.56.102"
  end

  config.vm.define "node3" do |node3|
    node3.vm.hostname = "node3"
    node3.vm.network "private_network", ip: "192.168.56.103"
  end

  config.vm.provider "virtualbox" do |vb|
    vb.memory = "1024" # 1GB RAM per machine
    vb.cpus = 2
  end
end