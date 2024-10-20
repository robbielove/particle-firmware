suite('SPI MASTER SLAVE');

platform('gen3', 'p2');
systemThread('enabled');
timeout(20 * 60 * 1000);

// FIXME: currently no way to define multiple fixture configurations within a test suite
fixture('spi1_slave', 'spi_master'); // use with spi_master_slave_1_bw.config.js and spi_master_slave_2_bw.config.js
// fixture('spi1_slave', 'spi1_master'); // use with spi_master_slave_3_bw.config.js

// This tag should be filtered out by default
tag('fixture');